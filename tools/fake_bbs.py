#!/usr/bin/env python3
"""A tiny telnet 'BBS' for testing JumiTerm.

It negotiates like a real BBS, draws an ANSI test screen, then echoes what
you type. Keys: f = flood 20 KB of ANSI text, s = redraw the test screen,
q = disconnect. File transfers run lrzsz on the connection:
d = YMODEM download of every file in --files, x = XMODEM-1K download of
TEST.BIN, u = YMODEM upload into --uploads, w = XMODEM upload into
--uploads/UPLOAD.BIN. Without --files, test files are generated.

  python3 tools/fake_bbs.py [--port 2323] [--files DIR] [--uploads DIR]
"""
import argparse
import hashlib
import os
import socket
import subprocess
import tempfile
import threading

IAC, DONT, DO, WONT, WILL, SB, SE = 255, 254, 253, 252, 251, 250, 240
BINARY, ECHO, SGA, TTYPE, NAWS = 0, 1, 3, 24, 31


def test_screen():
    e = "\x1b["
    out = [e + "0m" + e + "2J" + e + "H"]
    out.append(e + "1;33;44m\xc9" + "\xcd" * 78 + "\xbb")
    out.append(e + "2;1H\xba" + e + "0;1;37;44m" + " JumiTerm test screen".ljust(78) + e + "33m\xba")
    out.append(e + "3;1H\xc8" + "\xcd" * 78 + "\xbc" + e + "0m")
    for bg in range(8):
        out.append(e + "%d;3H" % (5 + bg))
        for fg in range(16):
            out.append(e + "0;%s3%d;4%dm Aa " % ("1;" if fg >= 8 else "", fg % 8, bg))
    out.append(e + "0m")
    out.append(e + "14;3HShades: \xb0\xb0\xb1\xb1\xb2\xb2\xdb\xdb  Halves: \xdc\xdf\xdd\xde")
    out.append(e + "15;3HSingle box: \xda\xc4\xc2\xc4\xbf \xc3\xc4\xc5\xc4\xb4 \xc0\xc4\xc1\xc4\xd9")
    out.append(e + "16;3HAccents: Hyv\x84\x84 p\x84iv\x84\x84! \x8e\x99\x86\x94\x81\x82")
    out.append(e + "25;71H80x25 OK")
    out.append(e + "20;3HType to echo. f = flood, s = redraw, q = quit" + e + "22;3H> ")
    return "".join(out).encode("latin-1")


def flood():
    lines = []
    n = 0
    while sum(len(x) for x in lines) < 20000:
        lines.append("\x1b[%dm Line %04d: the quick brown fox jumps over the lazy dog \x1b[0m\r\n"
                     % (31 + n % 7, n))
        n += 1
    return "".join(lines).encode("latin-1")


def read_for(conn, seconds):
    conn.settimeout(seconds)
    data = b""
    try:
        while True:
            chunk = conn.recv(1024)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    conn.settimeout(None)
    return data


def strip_telnet(data):
    out = bytearray()
    i = 0
    while i < len(data):
        b = data[i]
        if b != IAC:
            out.append(b)
            i += 1
        elif i + 1 < len(data) and data[i + 1] == SB:
            end = data.find(bytes([IAC, SE]), i)
            i = len(data) if end < 0 else end + 2
        else:
            i += 3 if i + 1 < len(data) and data[i + 1] >= WILL else 2
    return bytes(out)


def make_test_files(path):
    """Download test files: every byte value (IAC, CR, NUL, SUB, CAN ...) appears."""
    files = {
        "TEST.BIN": bytes((i * 7 + 3) & 0xFF for i in range(40000)),
        "Small text file.txt": b"Hello from the fake BBS.\r\n" * 12,
        "EMPTY.DAT": b"",
        "A very long file name that keeps on going to test the screen.txt": b"long name\r\n",
    }
    for name, data in files.items():
        with open(os.path.join(path, name), "wb") as f:
            f.write(data)


def digest(path):
    with open(path, "rb") as f:
        data = f.read()
    return "%d bytes sha256 %s" % (len(data), hashlib.sha256(data).hexdigest()[:16])


class Telnet:
    """Server-side telnet decoder for transfers: agrees to BINARY, un-doubles
    IAC, drops NUL after CR while the client is in text mode."""

    def __init__(self):
        self.state = "data"
        self.cmd = 0
        self.last_cr = False
        self.rx_binary = False
        self.answered = set()

    def decode(self, data):
        out, replies = bytearray(), bytearray()
        for b in data:
            if self.state == "data":
                if b == IAC:
                    self.state = "iac"
                elif b == 0 and self.last_cr and not self.rx_binary:
                    self.last_cr = False
                else:
                    self.last_cr = b == 13
                    out.append(b)
            elif self.state == "iac":
                if b == IAC:
                    out.append(IAC)
                    self.last_cr = False
                    self.state = "data"
                elif b >= WILL:
                    self.cmd = b
                    self.state = "opt"
                elif b == SB:
                    self.state = "sb"
                else:
                    self.state = "data"
            elif self.state == "opt":
                if self.cmd == WILL and b == BINARY:
                    self.rx_binary = True
                elif self.cmd == WONT and b == BINARY:
                    self.rx_binary = False
                if (self.cmd, b) not in self.answered and self.cmd in (WILL, DO):
                    self.answered.add((self.cmd, b))
                    ok = b == BINARY or (self.cmd == DO and b in (ECHO, SGA))
                    if self.cmd == WILL:
                        replies += bytes([IAC, DO if ok else DONT, b])
                    else:
                        replies += bytes([IAC, WILL if ok else WONT, b])
                self.state = "data"
            elif self.state == "sb":
                if b == IAC:
                    self.state = "sb_iac"
            elif self.state == "sb_iac":
                self.state = "data" if b == SE else "sb"
        return bytes(out), bytes(replies)


def escape(data):
    return data.replace(bytes([IAC]), bytes([IAC, IAC]))


def bridge(conn, argv, cwd, log):
    """Runs an lrzsz command with the connection as its stdin and stdout."""
    tn = Telnet()
    lock = threading.Lock()
    proc = subprocess.Popen(argv, cwd=cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL)

    def to_client():
        while True:
            data = os.read(proc.stdout.fileno(), 4096)
            if not data:
                return
            with lock:
                conn.sendall(escape(data))

    out = threading.Thread(target=to_client, daemon=True)
    out.start()
    conn.settimeout(0.2)
    try:
        while proc.poll() is None:
            try:
                data = conn.recv(4096)
            except socket.timeout:
                continue
            if not data:
                proc.kill()
                break
            payload, replies = tn.decode(data)
            if replies:
                with lock:
                    conn.sendall(replies)
            if payload:
                try:
                    proc.stdin.write(payload)
                    proc.stdin.flush()
                except BrokenPipeError:
                    pass
    finally:
        conn.settimeout(None)
        proc.wait()
        out.join(2)
        proc.stdin.close()
        proc.stdout.close()
    log("%s finished with exit code %d" % (argv[0], proc.returncode))
    return proc.returncode


def transfer(conn, key, files, uploads, log):
    names = sorted(os.listdir(files))
    if key == "d":
        argv, cwd, what = ["sb", "-q"] + names, files, "YMODEM download of %d files" % len(names)
    elif key == "x":
        argv, cwd, what = ["sx", "-q", "-k", "TEST.BIN"], files, "XMODEM download of TEST.BIN"
    elif key == "u":
        argv, cwd, what = ["rb", "-q", "-E"], uploads, "YMODEM upload"
    else:
        argv, cwd, what = ["rx", "-q", "-c", "UPLOAD.BIN"], uploads, "XMODEM upload to UPLOAD.BIN"
    conn.sendall(("\r\nStarting %s - start your transfer now.\r\n" % what).encode())
    log("transfer: %s" % what)
    if key in "dx":
        for n in (names if key == "d" else ["TEST.BIN"]):
            log("  sending %s: %s" % (n, digest(os.path.join(files, n))))
    if key == "w" and os.path.exists(os.path.join(uploads, "UPLOAD.BIN")):
        os.remove(os.path.join(uploads, "UPLOAD.BIN"))
    before = set(os.listdir(uploads))
    rc = bridge(conn, argv, cwd, log)
    for n in sorted(set(os.listdir(uploads)) - before):
        log("  received %s: %s" % (n, digest(os.path.join(uploads, n))))
    conn.sendall(("\r\nTransfer %s.\r\n> " % ("OK" if rc == 0 else "failed (%d)" % rc)).encode())


def handle(conn, log, files=None, uploads=None):
    conn.sendall(bytes([IAC, WILL, ECHO, IAC, WILL, SGA, IAC, DO, TTYPE, IAC, DO, NAWS]))
    log("negotiation reply: %s" % read_for(conn, 0.5).hex(" "))
    conn.sendall(bytes([IAC, SB, TTYPE, 1, IAC, SE]))
    log("ttype reply: %s" % read_for(conn, 0.5).hex(" "))
    conn.sendall(test_screen())
    while True:
        data = conn.recv(1024)
        if not data:
            return
        for b in strip_telnet(data):
            if b == ord("q"):
                conn.sendall(b"\r\nBye!\r\n")
                return
            if b == ord("f"):
                conn.sendall(flood())
            elif b == ord("s"):
                conn.sendall(test_screen())
            elif chr(b) in "dxuw" and files:
                transfer(conn, chr(b), files, uploads, log)
            elif b == 13:
                conn.sendall(b"\r\n> ")
            elif b != 0:
                conn.sendall(bytes([b]))


def serve(listener, log=print, files=None, uploads=None):
    while True:
        conn, addr = listener.accept()
        log("caller from %s:%d" % addr)

        def run(c=conn):
            try:
                handle(c, log, files, uploads)
            except OSError as e:
                log("connection error: %s" % e)
            finally:
                c.close()
                log("caller gone")

        threading.Thread(target=run, daemon=True).start()


def main():
    ap = argparse.ArgumentParser(description="Fake telnet BBS for JumiTerm tests")
    ap.add_argument("--port", type=int, default=2323)
    ap.add_argument("--files", help="directory offered for downloads (default: generated test files)")
    ap.add_argument("--uploads", help="directory that receives uploads (default: a temporary one)")
    args = ap.parse_args()
    files = args.files or tempfile.mkdtemp(prefix="bbs_files_")
    uploads = args.uploads or tempfile.mkdtemp(prefix="bbs_uploads_")
    if not args.files:
        make_test_files(files)
    print("downloads from %s, uploads to %s" % (files, uploads))
    listener = socket.socket()
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", args.port))
    listener.listen(5)
    print("fake BBS on 127.0.0.1:%d" % args.port)
    serve(listener, files=files, uploads=uploads)


if __name__ == "__main__":
    main()
