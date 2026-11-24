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
        """Open one live TCP RSP session and initialize acknowledged packet mode.

        Args:
            host: Probe hostname or IP address.
            port: Probe GDB TCP port.
        Raises:
            OSError: Connection setup fails; socket operations use five-second timeouts.
        """
        self.connection = socket.create_connection((host, port), timeout=5)
        self.connection.settimeout(5)
        self.console = []
        self.acknowledge = True

    def close(self):
        """Close the owned TCP socket; the caller must also close on assertion failure."""
        self.connection.close()

    def byte(self):
        """Receive one byte, raising ConnectionError if the peer has closed the session."""
        value = self.connection.recv(1)
        if not value:
            raise ConnectionError("Probe closed the RSP connection")
        return value

    def reply(self):
        """Validate a reply checksum and collect console packets until a response arrives.

        Returns:
            Response payload bytes without framing.
        Raises:
            AssertionError: Framing/checksum is invalid or a valid command was rejected.
            OSError: Receive/acknowledgement fails or times out.
        """
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
        """Send one ASCII RSP command and return its validated response payload.

        Args:
            payload: Unframed ASCII command text.
            fragmented: Send three-byte fragments to exercise TCP stream reconstruction.
        Returns:
            Response bytes; asynchronous console text is appended to self.console.
        """
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
    """Exercise packet checksums, session exclusivity, SWD scan and reconnect behavior.

    Requires a live probe; SWD scan may drive the target interface. Assertion and socket
    errors propagate to fail the test process, and opened sessions are closed in finally.
    """
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
