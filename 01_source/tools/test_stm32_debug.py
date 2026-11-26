#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check Wi-Fi/SWD on a powered STM32F103 without programming flash or writing RAM.

The default test attaches, reads identity/registers/memory, and detaches (resumes the
application). --exercise-core also single-steps, tests a hardware breakpoint, and
continues/interrupts the existing application. Close other debug clients first.
"""

import argparse
import hashlib
import json
import socket
import time

from test_gdb_transport import Remote


def monitor(client, command):
    """Run one BMD monitor command, returning its reply and console output."""
    start = len(client.console)
    reply = client.command("qRcmd," + command.encode("ascii").hex())
    output = "".join(client.console[start:])
    print(output.rstrip(), flush=True)
    return reply, output


def memory(client, address, length):
    """Read exactly length bytes through RSP; reject errors and truncated replies."""
    reply = client.command(f"m{address:x},{length:x}")
    assert len(reply) == length * 2, f"Read 0x{address:08x} failed: {reply!r}"
    return bytes.fromhex(reply.decode("ascii"))


def send(client, payload):
    """Send an RSP packet without waiting, for continue followed by an interrupt."""
    encoded = payload.encode("ascii")
    client.connection.sendall(b"$" + encoded + f"#{sum(encoded) % 256:02x}".encode("ascii"))


def stopped(reply):
    """Require a normal target stop, rejecting transport errors and target exits."""
    assert reply.startswith((b"T", b"S")), f"Target did not stop normally: {reply!r}"


def run_until_stop(client, command):
    """Run/step and interrupt on timeout so cleanup can operate on a halted CPU."""
    send(client, command)
    try:
        reply = client.reply()
    except socket.timeout:
        client.connection.sendall(b"\x03")
        stopped(client.reply())
        raise
    stopped(reply)


def exercise_core(client):
    """Test execution with hardware debug registers only; never patch target code."""
    run_until_stop(client, "s")
    registers = bytes.fromhex(client.command("g").decode("ascii"))
    assert len(registers) >= 64, "Missing Cortex-M PC register"
    pc = int.from_bytes(registers[60:64], "little") & ~1
    breakpoint = f"1,{pc:x},2"
    assert client.command("Z" + breakpoint) == b"OK", "Hardware breakpoint rejected"
    try:
        run_until_stop(client, "c")
        halted = bytes.fromhex(client.command("g").decode("ascii"))
        actual_pc = int.from_bytes(halted[60:64], "little") & ~1
        assert actual_pc == pc, f"Breakpoint PC 0x{actual_pc:x} != 0x{pc:x}"
    finally:
        assert client.command("z" + breakpoint) == b"OK", "Hardware breakpoint cleanup failed"
    send(client, "c")
    time.sleep(0.2)
    client.connection.sendall(b"\x03")
    stopped(client.reply())
    print("PASS single-step, hardware breakpoint and continue/interrupt", flush=True)


def session(args):
    """Validate one real target session and release the target on every exit path."""
    client = Remote(args.host, args.port)
    client.connection.settimeout(10)
    attached = False
    try:
        assert b"PacketSize=" in client.command("qSupported", fragmented=True)
        assert monitor(client, "connect_rst disable")[0] == b"OK"
        assert monitor(client, f"frequency {args.clock}")[0] == b"OK"
        reply, scan = monitor(client, "swd_scan")
        assert reply == b"OK", f"SWD scan failed: {scan.strip()}"
        stopped(client.command("vAttach;1"))
        attached = True
        registers = bytes.fromhex(client.command("g").decode("ascii"))
        assert len(registers) >= 68, "Cortex-M general registers missing"
        cpuid = int.from_bytes(memory(client, 0xE000ED00, 4), "little")
        device = int.from_bytes(memory(client, 0xE0042000, 4), "little")
        flash_kib = int.from_bytes(memory(client, 0x1FFFF7E0, 2), "little")
        assert cpuid & 0xFF00FFF0 == 0x4100C230, f"Not Cortex-M3: 0x{cpuid:08x}"
        assert device & 0xFFF == 0x410, f"Not medium-density STM32F1: 0x{device:08x}"
        assert flash_kib in (64, 128), f"Unexpected flash capacity: {flash_kib} KiB"
        vectors = memory(client, 0x08000000, 64)
        for _ in range(10):
            assert memory(client, 0x08000000, 64) == vectors, "Unstable flash read"
            memory(client, 0x20000000, 64)
        print(json.dumps({"cpuid": f"0x{cpuid:08x}", "device_id": f"0x{device:08x}",
                          "flash_kib": flash_kib, "register_bytes": len(registers),
                          "vector_sha256": hashlib.sha256(vectors).hexdigest()}), flush=True)
        print("PASS attach, identity, registers and repeated flash/RAM reads", flush=True)
        if args.exercise_core:
            exercise_core(client)
    finally:
        try:
            if attached:
                assert client.command("D") == b"OK", "Target detach failed"
        finally:
            client.close()


def main():
    """Run a bounded number of sessions; a failed assertion makes the test fail."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host")
    parser.add_argument("--port", type=int, default=2022)
    parser.add_argument("--clock", type=int, default=400000, metavar="1000..8000000")
    parser.add_argument("--sessions", type=int, default=1)
    parser.add_argument("--exercise-core", action="store_true")
    args = parser.parse_args()
    if not 1000 <= args.clock <= 8000000:
        parser.error("--clock must be between 1000 and 8000000 Hz")
    if not 1 <= args.sessions <= 100:
        parser.error("--sessions must be between 1 and 100")
    for index in range(args.sessions):
        session(args)
        print(f"PASS target session {index + 1}/{args.sessions}", flush=True)
        time.sleep(0.2)


if __name__ == "__main__":
    main()
