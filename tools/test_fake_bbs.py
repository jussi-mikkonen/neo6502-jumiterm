"""Tests for the fake BBS telnet decoder and its lrzsz transfers. The
transfer tests play the JumiTerm side with build/xfer_host behind a small
telnet client, so they also exercise src/xmodem.c over telnet."""
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import fake_bbs  # noqa: E402
from fake_bbs import BINARY, DO, DONT, IAC, SB, SE, WILL, WONT  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.environ.get("XFER_HOST", os.path.join(HERE, "..", "build", "xfer_host"))
HAVE = all(shutil.which(t) for t in ("sx", "rx", "sb", "rb")) and os.path.exists(HOST)
QUIET = lambda *a: None  # noqa: E731


class TelnetDecodeTest(unittest.TestCase):
    def test_iac_iac_is_data(self):
        self.assertEqual(fake_bbs.Telnet().decode(bytes([1, IAC, IAC, 2])), (bytes([1, IAC, 2]), b""))

    def test_binary_agreed_once(self):
        tn = fake_bbs.Telnet()
        _, r = tn.decode(bytes([IAC, WILL, BINARY, IAC, DO, BINARY, IAC, WILL, BINARY]))
        self.assertEqual(r, bytes([IAC, DO, BINARY, IAC, WILL, BINARY]))
        self.assertTrue(tn.rx_binary)

    def test_other_options_refused(self):
        _, r = fake_bbs.Telnet().decode(bytes([IAC, WILL, 5, IAC, DO, 6]))
        self.assertEqual(r, bytes([IAC, DONT, 5, IAC, WONT, 6]))

    def test_cr_nul_only_in_text_mode(self):
        tn = fake_bbs.Telnet()
        self.assertEqual(tn.decode(b"a\r\x00b")[0], b"a\rb")
        tn.decode(bytes([IAC, WILL, BINARY]))
        self.assertEqual(tn.decode(b"a\r\x00b")[0], b"a\r\x00b")

    def test_subnegotiation_skipped(self):
        data, _ = fake_bbs.Telnet().decode(bytes([7, IAC, SB, 24, 0, 65, IAC, SE, 8]))
        self.assertEqual(data, bytes([7, 8]))

    def test_escape(self):
        self.assertEqual(fake_bbs.escape(bytes([1, IAC, 2])), bytes([1, IAC, IAC, 2]))


def start_bbs(files, uploads):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", 0))
    s.listen(5)
    threading.Thread(target=fake_bbs.serve, args=(s, QUIET, files, uploads), daemon=True).start()
    return s.getsockname()[1]


def client_decode(data, state):
    """Client-side IAC handling: un-double IAC, drop negotiation."""
    out = bytearray()
    for b in data:
        st = state[0]
        if st == "data":
            if b == IAC:
                state[0] = "iac"
            else:
                out.append(b)
        elif st == "iac":
            if b == IAC:
                out.append(IAC)
                state[0] = "data"
            elif b >= WILL:
                state[0] = "opt"
            elif b == SB:
                state[0] = "sb"
            else:
                state[0] = "data"
        elif st == "opt":
            state[0] = "data"
        elif st == "sb":
            if b == IAC:
                state[0] = "sb_iac"
        elif st == "sb_iac":
            state[0] = "data" if b == SE else "sb"
    return bytes(out)


def run_client(port, key, argv, cwd):
    """Connects, asks for BINARY, presses key, then runs argv as the
    terminal's transfer with telnet encoding in between."""
    c = socket.create_connection(("127.0.0.1", port))
    c.sendall(bytes([IAC, WILL, BINARY, IAC, DO, BINARY]))
    time.sleep(1.2)                       # the BBS reads negotiation answers for 1 s
    c.settimeout(0.5)
    try:
        while c.recv(65536):
            pass
    except socket.timeout:
        pass
    c.sendall(key.encode())
    time.sleep(0.3)
    try:
        c.recv(65536)                     # "Starting ... start your transfer now."
    except socket.timeout:
        pass
    c.settimeout(None)
    proc = subprocess.Popen(argv, cwd=cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    state = ["data"]

    def to_proc():
        while proc.poll() is None:
            try:
                data = c.recv(4096)
            except OSError:
                return
            if not data:
                return
            try:
                proc.stdin.write(client_decode(data, state))
                proc.stdin.flush()
            except (BrokenPipeError, ValueError):
                return

    threading.Thread(target=to_proc, daemon=True).start()
    while True:
        data = os.read(proc.stdout.fileno(), 4096)
        if not data:
            break
        c.sendall(fake_bbs.escape(data))
    err = proc.communicate(timeout=60)[1].decode()
    time.sleep(0.5)
    c.close()
    return proc.returncode, err


@unittest.skipUnless(HAVE, "needs lrzsz and build/xfer_host")
class BbsTransferTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.files = os.path.join(self.tmp, "files")
        self.uploads = os.path.join(self.tmp, "uploads")
        self.local = os.path.join(self.tmp, "local")
        for d in (self.files, self.uploads, self.local):
            os.mkdir(d)
        fake_bbs.make_test_files(self.files)
        self.port = start_bbs(self.files, self.uploads)

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def same(self, a, b):
        with open(a, "rb") as f, open(b, "rb") as g:
            self.assertEqual(f.read(), g.read(), b)

    def test_ymodem_download(self):
        rc, err = run_client(self.port, "d", [HOST, "recv", "y"], self.local)
        self.assertEqual(rc, 0, err)
        for n in os.listdir(self.files):
            self.same(os.path.join(self.files, n), os.path.join(self.local, n))

    def test_xmodem_download(self):
        rc, err = run_client(self.port, "x", [HOST, "recv", "x", "T.BIN"], self.local)
        self.assertEqual(rc, 0, err)
        self.same(os.path.join(self.files, "TEST.BIN"), os.path.join(self.local, "T.BIN"))

    def test_ymodem_upload(self):
        with open(os.path.join(self.local, "up.dat"), "wb") as f:
            f.write(bytes(range(256)) * 50)
        rc, err = run_client(self.port, "u", [HOST, "send", "y", "up.dat"], self.local)
        self.assertEqual(rc, 0, err)
        time.sleep(0.5)
        self.same(os.path.join(self.local, "up.dat"), os.path.join(self.uploads, "up.dat"))

    def test_xmodem_upload(self):
        data = bytes(range(256)) * 8      # a multiple of 128: no padding to strip
        with open(os.path.join(self.local, "up.dat"), "wb") as f:
            f.write(data)
        rc, err = run_client(self.port, "w", [HOST, "send", "1k", "up.dat"], self.local)
        self.assertEqual(rc, 0, err)
        time.sleep(0.5)
        with open(os.path.join(self.uploads, "UPLOAD.BIN"), "rb") as f:
            self.assertEqual(f.read(), data)


if __name__ == "__main__":
    unittest.main()
