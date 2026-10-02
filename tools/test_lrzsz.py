#!/usr/bin/env python3
"""Interop tests: src/xmodem.c (through build/xfer_host) against lrzsz and
against itself. Skipped when lrzsz or xfer_host is missing.

  make build/xfer_host && python3 -m unittest tools/test_lrzsz.py
"""
import os
import shutil
import subprocess
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.environ.get("XFER_HOST", os.path.join(HERE, "..", "build", "xfer_host"))
SIZES = [0, 1, 127, 128, 129, 1023, 1024, 1025, 50000]
HAVE = all(shutil.which(t) for t in ("sx", "rx", "sb", "rb")) and os.path.exists(HOST)


def run_pair(a, a_cwd, b, b_cwd, timeout=60):
    """Runs a and b with a's stdout wired to b's stdin and back."""
    r1, w1 = os.pipe()
    r2, w2 = os.pipe()
    pa = subprocess.Popen(a, cwd=a_cwd, stdin=r1, stdout=w2, stderr=subprocess.PIPE)
    pb = subprocess.Popen(b, cwd=b_cwd, stdin=r2, stdout=w1, stderr=subprocess.PIPE)
    for fd in (r1, w1, r2, w2):
        os.close(fd)
    try:
        ea = pa.communicate(timeout=timeout)[1]
        eb = pb.communicate(timeout=timeout)[1]
    finally:
        pa.kill()
        pb.kill()
    return pa.returncode, ea.decode(errors="replace"), pb.returncode, eb.decode(errors="replace")


def payload(n, seed=0):
    # Every byte value, including SUB, CAN, IAC and CR, at varying offsets.
    # XMODEM tests replace SUB (0x1A): a receiver cannot tell trailing SUB
    # data from padding, so ours trims it and lrzsz rx keeps the padding.
    return bytes((i * 7 + seed) & 0xFF for i in range(n))


@unittest.skipUnless(HAVE, "needs lrzsz (sx rx sb rb) and build/xfer_host")
class Interop(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.src = os.path.join(self.tmp, "src")
        self.dst = os.path.join(self.tmp, "dst")
        os.mkdir(self.src)
        os.mkdir(self.dst)

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def make(self, name, data):
        with open(os.path.join(self.src, name), "wb") as f:
            f.write(data)

    def got(self, name):
        with open(os.path.join(self.dst, name), "rb") as f:
            return f.read()

    def check(self, result):
        ra, ea, rb, eb = result
        self.assertEqual((ra, rb), (0, 0), "sender: %s\nreceiver: %s" % (ea, eb))

    # --- downloads (our receiver) ---

    def test_recv_xmodem_from_sx(self):
        for n in SIZES:
            with self.subTest(size=n):
                data = payload(n, n).replace(b"\x1a", b"\x1b") if n else b""
                self.make("f.bin", data)
                self.check(run_pair(["sx", "-q", "f.bin"], self.src, [HOST, "recv", "x", "out.bin"], self.dst))
                self.assertEqual(self.got("out.bin"), data)

    def test_recv_xmodem_1k_from_sx(self):
        data = payload(50000).replace(b"\x1a", b"\x1b")
        self.make("f.bin", data)
        self.check(run_pair(["sx", "-q", "-k", "f.bin"], self.src, [HOST, "recv", "x", "out.bin"], self.dst))
        self.assertEqual(self.got("out.bin"), data)

    def test_recv_ymodem_batch_from_sb(self):
        files = {"a.bin": payload(0), "b.bin": payload(129, 3), "c.zip": payload(50000, 5)}
        for k, v in files.items():
            self.make(k, v)
        self.check(run_pair(["sb", "-q"] + list(files), self.src, [HOST, "recv", "y"], self.dst))
        for k, v in files.items():
            self.assertEqual(self.got(k), v, k)

    def test_recv_ymodem_sizes_from_sb(self):
        for n in SIZES:
            with self.subTest(size=n):
                data = payload(n, n)
                self.make("s%d.bin" % n, data)
                self.check(run_pair(["sb", "-q", "s%d.bin" % n], self.src, [HOST, "recv", "y"], self.dst))
                self.assertEqual(self.got("s%d.bin" % n), data)

    # --- uploads (our sender) ---

    def test_send_xmodem_to_rx(self):
        for proto in ("x", "1k"):
            for n in SIZES:
                with self.subTest(proto=proto, size=n):
                    data = payload(n, n)
                    self.make("f.bin", data)
                    self.check(run_pair([HOST, "send", proto, "f.bin"], self.src, ["rx", "-q", "-c", "out.bin"], self.dst))
                    got = self.got("out.bin")
                    self.assertEqual(got[:n], data)
                    self.assertEqual(got[n:], b"\x1a" * (len(got) - n))

    def test_send_xmodem_checksum_to_rx(self):
        data = payload(3000)
        self.make("f.bin", data)
        self.check(run_pair([HOST, "send", "1k", "f.bin"], self.src, ["rx", "-q", "out.bin"], self.dst))
        self.assertEqual(self.got("out.bin")[:3000], data)

    def test_send_ymodem_batch_to_rb(self):
        files = {"a.bin": payload(0), "b.bin": payload(129, 3), "c.zip": payload(50000, 5)}
        for k, v in files.items():
            self.make(k, v)
        self.check(run_pair([HOST, "send", "y"] + list(files), self.src, ["rb", "-q"], self.dst))
        for k, v in files.items():
            self.assertEqual(self.got(k), v, k)

    # --- ourselves on both ends ---

    def test_loopback(self):
        for proto in ("x", "1k", "y"):
            for n in SIZES:
                with self.subTest(proto=proto, size=n):
                    data = payload(n, n)
                    if proto != "y":
                        data = data.replace(b"\x1a", b"\x1b")
                    self.make("f.bin", data)
                    recv = [HOST, "recv", "y"] if proto == "y" else [HOST, "recv", "x", "f.bin"]
                    self.check(run_pair([HOST, "send", proto, "f.bin"], self.src, recv, self.dst))
                    self.assertEqual(self.got("f.bin"), data)


if __name__ == "__main__":
    unittest.main()
