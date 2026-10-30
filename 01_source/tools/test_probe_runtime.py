#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check persisted Wi-Fi and idle/busy clock configuration on a real probe."""

import argparse
from pathlib import Path
import re
import time

import serial

from test_gdb_transport import Remote


def collect(port, seconds):
    deadline = time.monotonic() + seconds
    data = bytearray()
    while time.monotonic() < deadline:
        data.extend(port.read(4096))
    return data.decode("utf-8", errors="replace")


def command(port, text):
    port.write((text + "\n").encode("ascii"))
    port.flush()
    return collect(port, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--ssid", required=True)
    parser.add_argument("--reboot", action="store_true", help="Reset via USB UART RTS")
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    log = ""
    with serial.Serial(args.serial, 115200, timeout=0.1) as port:
        if args.reboot:
            port.dtr = False
            port.rts = True
            time.sleep(0.15)
            port.rts = False
        log += collect(port, 18 if args.reboot else 1)
        status = command(port, "status")
        log += status
        match = re.search(r"STATUS wifi=STA ssid=(.*?) ip=([0-9.]+)", status)
        assert match and match[1] == args.ssid, status
        host = match[2]
        print(f"PASS stored Wi-Fi profile: {args.ssid}, IP {host}")
        response = command(port, "clock 1000000")
        log += response
        assert "CLOCK_CONFIG ESP_OK" in response, response
        status = command(port, "status")
        log += status
        assert "swd=1000000" in status, status
        print("PASS idle clock applies on SWD owner")
        client = Remote(host, 2022)
        try:
            assert b"PacketSize=" in client.command("qSupported")
            response = command(port, "clock 200000")
            log += response
            assert "CLOCK_CONFIG ESP_ERR_INVALID_STATE" in response, response
            status = command(port, "status")
            log += status
            assert "gdb=BUSY" in status and "swd=1000000" in status, status
            print("PASS clock modification rejected during a GDB session")
        finally:
            client.close()
        log += collect(port, 0.5)
        response = command(port, "clock 400000")
        log += response
        assert "CLOCK_CONFIG ESP_OK" in response, response
        status = command(port, "status")
        log += status
        assert "gdb=IDLE" in status and "swd=400000" in status, status
        assert not re.search(r"Guru Meditation|assert failed|Backtrace:|watchdog got triggered", log)
        print(status.strip())
        print("PASS session release and clock restore; no panic/watchdog in captured log")
    args.log.parent.mkdir(parents=True, exist_ok=True)
    args.log.write_text(log, encoding="utf-8")


if __name__ == "__main__":
    main()
