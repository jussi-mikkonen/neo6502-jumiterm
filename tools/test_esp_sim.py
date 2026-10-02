import os
import socket
import sys
import threading
import time
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import esp_sim  # noqa: E402
import fake_bbs  # noqa: E402

QUIET = lambda *a: None  # noqa: E731


def start_listener(serve):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", 0))
    s.listen(5)
    threading.Thread(target=serve, args=(s,), daemon=True).start()
    return s.getsockname()[1]


class Client:
    def __init__(self, port):
        self.s = socket.create_connection(("127.0.0.1", port))
        self.s.settimeout(5)
        self.buf = b""

    def cmd(self, text):
        self.s.sendall(text.encode() + b"\r\n")

    def expect(self, token):
        while token not in self.buf:
            data = self.s.recv(4096)
            if not data:
                raise AssertionError("connection closed waiting for %r" % token)
            self.buf += data
        i = self.buf.index(token) + len(token)
        out, self.buf = self.buf[:i], self.buf[i:]
        return out


class EspSimTest(unittest.TestCase):
    def setUp(self):
        self.bbs_port = start_listener(lambda s: fake_bbs.serve(s, log=QUIET))
        self.sim_port = start_listener(lambda s: esp_sim.EspSim(log=QUIET).serve(s))
        self.c = Client(self.sim_port)

    def test_basic_commands(self):
        c = self.c
        c.cmd("AT")
        c.expect(b"OK\r\n")
        c.cmd("ATE0")
        c.expect(b"OK\r\n")
        c.cmd("AT+CWSTATE?")
        self.assertIn(b"+CWSTATE:2", c.expect(b"OK\r\n"))
        c.cmd("AT+UART_CUR=38400,8,1,0,0")
        c.expect(b"OK\r\n")
        c.cmd("AT+BOGUS")
        c.expect(b"ERROR\r\n")

    def test_transparent_session_and_escape(self):
        c = self.c
        c.cmd("ATE0")
        c.expect(b"OK\r\n")
        c.cmd("AT+CIPMODE=1")
        c.expect(b"OK\r\n")
        c.cmd('AT+CIPSTART="TCP","127.0.0.1",%d' % self.bbs_port)
        c.expect(b"CONNECT")
        c.expect(b"OK\r\n")
        c.cmd("AT+CIPSEND")
        c.expect(b">")
        c.expect(bytes([255, 253, 24]))   # the BBS asks DO TTYPE
        c.expect(b"80x25 OK")             # the test screen arrived
        c.s.sendall(b"z")
        c.expect(b"z")
        time.sleep(0.7)
        c.s.sendall(b"+++")
        time.sleep(0.7)
        c.cmd("AT+CIPCLOSE")
        self.assertIn(b"CLOSED", c.expect(b"OK\r\n"))

    def test_plus_inside_data_is_forwarded(self):
        c = self.c
        c.cmd("AT+CIPMODE=1")
        c.expect(b"OK\r\n")
        c.cmd('AT+CIPSTART="TCP","127.0.0.1",%d' % self.bbs_port)
        c.expect(b"OK\r\n")
        c.cmd("AT+CIPSEND")
        c.expect(b">")
        c.expect(b"80x25 OK")
        c.s.sendall(b"a+++b")
        c.expect(b"a+++b")


class NoWifiTest(unittest.TestCase):
    def test_reports_no_wifi_and_refuses_connect(self):
        port = start_listener(lambda s: esp_sim.EspSim(wifi=False, log=QUIET).serve(s))
        c = Client(port)
        c.cmd("AT+CWSTATE?")
        self.assertIn(b"+CWSTATE:0", c.expect(b"OK\r\n"))
        c.cmd('AT+CIPSTART="TCP","127.0.0.1",1')
        c.expect(b"ERROR\r\n")


class FakeBbsTest(unittest.TestCase):
    def test_screen_and_flood(self):
        screen = fake_bbs.test_screen()
        self.assertIn(b"\x1b[2J", screen)
        self.assertIn(b"80x25 OK", screen)
        self.assertGreaterEqual(len(fake_bbs.flood()), 20000)


if __name__ == "__main__":
    unittest.main()
