#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise a live probe's GDB transport without requiring a connected target."""

import argparse
import socket
import time


class Remote:
    """Minimal acknowledged RSP client with checksum verification."""

    def __init__(self, host, port):
        self.connection = socket.create_connection((host, port), timeout=5)
        self.connection.settimeout(5)
        self.console = []
        self.acknowledge = True

    def close(self):
        self.connection.close()

    def byte(self):
        value = self.connection.recv(1)
        if not value:
            raise ConnectionError("Probe closed the RSP connection")
        return value

    def reply(self):
        while True:
            lead = self.byte()
            if lead in (b"+", b"-"):
                if lead == b"-":
                    raise AssertionError("Probe rejected a valid packet")
                continue
            assert lead == b"$", f"Unexpected packet prefix: {lead!r}"
            payload = bytearray()
            while True:
                value = self.byte()
                if value == b"#":
                    break
                payload.extend(value)
            checksum = self.byte() + self.byte()
            assert int(checksum, 16) == sum(payload) % 256, "Invalid reply checksum"
            if self.acknowledge:
                self.connection.sendall(b"+")
            if payload.startswith(b"O") and payload != b"OK":
                self.console.append(bytes.fromhex(payload[1:].decode()).decode(errors="replace"))
                continue
            return bytes(payload)

    def command(self, payload, fragmented=False):
        payload = payload.encode("ascii")
        packet = b"$" + payload + f"#{sum(payload) % 256:02x}".encode("ascii")
        if fragmented:
            for offset in range(0, len(packet), 3):
                self.connection.sendall(packet[offset:offset + 3])
                time.sleep(0.005)
        else:
            self.connection.sendall(packet)
        return self.reply()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host")
    parser.add_argument("--port", type=int, default=2022)
    parser.add_argument("--sessions", type=int, default=20)
    parser.add_argument("--expect-no-target", action="store_true")
    args = parser.parse_args()
    client = Remote(args.host, args.port)
    try:
        supported = client.command("qSupported", fragmented=True)
        assert b"PacketSize=" in supported, supported
        print("PASS fragmented qSupported:", supported.decode())
        client.connection.sendall(b"$qSupported#00")
        assert client.byte() == b"-", "Bad checksum must receive NACK"
        print("PASS checksum rejection")
        assert b"PacketSize=" in client.command("qSupported")
        second = socket.create_connection((args.host, args.port), timeout=5)
        second.settimeout(3)
        try:
            assert second.recv(1) == b"", "Second client was not rejected"
        except ConnectionResetError:
            pass
        finally:
            second.close()
        print("PASS single-client ownership")
        scan = "qRcmd," + b"swd_scan".hex()
        result = client.command(scan)
        output = "".join(client.console)
        print("SWD scan reply:", result.decode(), "console:", output.strip())
        if args.expect_no_target:
            assert "SWD scan failed" in output, "Unexpected target detected"
        assert b"PacketSize=" in client.command("qSupported"), "Scan lost the session"
        print("PASS transport remains responsive after SWD scan")
        assert client.command("QStartNoAckMode") == b"OK"
        client.acknowledge = False
        assert client.command("qC") == b"QC1"
        print("PASS no-ack negotiation")
    finally:
        client.close()
    for _ in range(args.sessions):
        time.sleep(0.15)
        client = Remote(args.host, args.port)
        try:
            assert b"PacketSize=" in client.command("qSupported")
        finally:
            client.close()
    print(f"PASS {args.sessions} abrupt disconnect/reconnect sessions")


if __name__ == "__main__":
    main()
