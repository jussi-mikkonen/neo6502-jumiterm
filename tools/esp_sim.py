#!/usr/bin/env python3
"""ESP-AT (ESP8266, AT 2.x) simulator for running JumiTerm in the Neo6502 emulator.

The emulator's UART connects to 127.0.0.1:25232 as a TCP client. This script
listens there and answers the AT commands JumiTerm uses, including
transparent mode (AT+CIPMODE=1 + AT+CIPSEND) and the '+++' escape. Like the
real firmware, it stays silent in transparent mode when the remote end closes.

  python3 tools/esp_sim.py [--port 25232] [--no-wifi]
"""
import argparse
import re
import select
import socket
import time

GUARD = 0.5  # seconds of silence required around '+++'


class Session:
    def __init__(self, conn, wifi, log):
        self.conn, self.wifi, self.log = conn, wifi, log
        self.echo = True
        self.cipmode = 0
        self.remote = None
        self.transparent = False
        self.line = b""
        self.plus = b""
        self.plus_time = None
        self.last_rx = time.monotonic()

    def send(self, data):
        if isinstance(data, str):
            data = data.encode("latin-1")
        self.conn.sendall(data)

    def ok(self):
        self.send("\r\nOK\r\n")

    def error(self):
        self.send("\r\nERROR\r\n")

    def forward(self, data):
        if self.remote:
            self.remote.sendall(data)

    def run(self):
        while True:
            # With CIPMODE=1 the real firmware only delivers remote data once
            # passthrough is active (after AT+CIPSEND); until then it stays
            # queued in the kernel socket buffer.
            poll_remote = self.remote is not None and (self.transparent or self.cipmode == 0)
            socks = [self.conn] + ([self.remote] if poll_remote else [])
            ready, _, _ = select.select(socks, [], [], 0.05)
            now = time.monotonic()
            if self.conn in ready:
                data = self.conn.recv(4096)
                if not data:
                    return
                self.from_host(data, now)
            if poll_remote and self.remote is not None and self.remote in ready:
                data = self.remote.recv(4096)
                if data:
                    self.send(data if self.transparent else b"+IPD,%d:" % len(data) + data)
                else:
                    self.log("remote closed")
                    self.remote.close()
                    self.remote = None
                    if not self.transparent:
                        self.send("CLOSED\r\n")
            if self.plus_time is not None and now - self.plus_time >= GUARD:
                self.log("+++ received: leaving transparent mode")
                self.transparent = False
                self.plus = b""
                self.plus_time = None

    def from_host(self, data, now):
        if self.transparent:
            for b in data:
                ch = bytes([b])
                quiet_before = self.plus or now - self.last_rx >= GUARD
                if ch == b"+" and quiet_before and len(self.plus) < 3:
                    self.plus += ch
                    if len(self.plus) == 3:
                        self.plus_time = now
                else:
                    if self.plus:
                        self.forward(self.plus)
                        self.plus = b""
                        self.plus_time = None
                    self.forward(ch)
                self.last_rx = now
            return
        self.last_rx = now
        self.line += data
        while b"\r\n" in self.line:
            cmd, self.line = self.line.split(b"\r\n", 1)
            self.command(cmd.decode("latin-1").strip())
            if self.transparent:
                rest, self.line = self.line, b""
                if rest:
                    self.from_host(rest, now)
                return

    def command(self, cmd):
        self.log("AT> " + cmd)
        if self.echo and cmd:
            self.send(cmd + "\r\n")
        up = cmd.upper()
        if up in ("AT", "AT+RST", "AT+CIPMUX=0") or up.startswith("AT+UART_CUR="):
            self.ok()
        elif up == "ATE0":
            self.echo = False
            self.ok()
        elif up == "ATE1":
            self.echo = True
            self.ok()
        elif up == "AT+GMR":
            self.send("AT version:2.2.0.0(simulator)\r\n")
            self.ok()
        elif up == "AT+CWSTATE?":
            self.send('+CWSTATE:%d,"%s"\r\n' % ((2, "simnet") if self.wifi else (0, "")))
            self.ok()
        elif up == "AT+CIPSTATUS":
            self.send("STATUS:%d\r\n" % (3 if self.remote else 2 if self.wifi else 5))
            self.ok()
        elif up.startswith("AT+CIPMODE="):
            self.cipmode = 1 if up.endswith("1") else 0
            self.ok()
        elif up.startswith("AT+CIPSTART="):
            self.cipstart(cmd)
        elif up == "AT+CIPSEND":
            if self.cipmode == 1 and self.remote:
                self.send("\r\nOK\r\n\r\n>")
                self.transparent = True
                self.last_rx = time.monotonic()
            else:
                self.error()
        elif up == "AT+CIPCLOSE":
            if self.remote:
                self.remote.close()
                self.remote = None
                self.send("CLOSED\r\n")
                self.ok()
            else:
                self.error()
        else:
            self.error()

    def cipstart(self, cmd):
        m = re.match(r'AT\+CIPSTART="TCP","([^"]+)",(\d+)$', cmd, re.I)
        if not self.wifi or not m:
            return self.error()
        if self.remote:
            self.send("ALREADY CONNECTED\r\n")
            return self.error()
        try:
            self.remote = socket.create_connection((m.group(1), int(m.group(2))), timeout=5)
            self.remote.settimeout(None)
        except OSError as e:
            self.log("connect failed: %s" % e)
            self.remote = None
            return self.error()
        self.send("CONNECT\r\n")
        self.ok()


class EspSim:
    def __init__(self, wifi=True, log=print):
        self.wifi = wifi
        self.log = log

    def serve(self, listener):
        while True:
            conn, _ = listener.accept()
            self.log("emulator connected")
            try:
                Session(conn, self.wifi, self.log).run()
            except OSError as e:
                self.log("session ended: %s" % e)
            finally:
                conn.close()


def main():
    ap = argparse.ArgumentParser(description="ESP-AT simulator for the Neo6502 emulator")
    ap.add_argument("--port", type=int, default=25232)
    ap.add_argument("--no-wifi", action="store_true", help="report Wi-Fi as not connected")
    args = ap.parse_args()
    listener = socket.socket()
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", args.port))
    listener.listen(1)
    print("ESP-AT simulator on 127.0.0.1:%d" % args.port)
    EspSim(wifi=not args.no_wifi).serve(listener)


if __name__ == "__main__":
    main()
