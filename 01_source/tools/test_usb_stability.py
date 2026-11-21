#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
# SPDX-License-Identifier: GPL-3.0-or-later
"""Measure native USB COM continuity and read-only flash data integrity.

Passive/open-close phases do not intentionally reset the board or send data.
The bulk-read phase deliberately enters ROM, uploads a RAM stub, reads flash,
    then attempts to reset into the application. It never writes flash, NVS, or an SD card.
Run with the project's ESP-IDF Python environment; close other serial monitors.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import time

import serial
from serial.tools import list_ports


BOOT_PATTERN = re.compile(r"ESP-ROM:|Board bring-up passed:")
PANIC_PATTERN = re.compile(
    r"Guru Meditation|abort\(\)|assert failed|Backtrace:|Brownout|watchdog got triggered",
    re.IGNORECASE,
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial-identity", required=True)
    parser.add_argument("--duration", type=float, default=300)
    parser.add_argument("--cycles", type=int, default=50)
    parser.add_argument("--reads", type=int, default=3)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.duration <= 3600 or not 1 <= args.cycles <= 500 or not 1 <= args.reads <= 20:
        parser.error("duration must be 1..3600 s, cycles 1..500, reads 1..20")
    args.output.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).resolve().parents[1]
    firmware = (source / "build" / "stm32_wifi_probe.bin").read_bytes()
    expected_hash = hashlib.sha256(firmware).hexdigest()
    identity = args.serial_identity.upper()
    start = time.monotonic()
    summary = {
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "identity": identity,
        "vid": "303A", "pid": "1001",
        "firmware_bytes": len(firmware), "firmware_sha256": expected_hash,
        "port_names": [], "events": [],
    }
    phase_data = {}

    def event(kind, **details):
        record = {"at_s": round(time.monotonic() - start, 3), "kind": kind, **details}
        summary["events"].append(record)
        print(json.dumps(record), flush=True)

    def find_port():
        matches = [p for p in list_ports.comports()
                   if p.vid == 0x303A and p.pid == 0x1001
                   and (p.serial_number or "").upper() == identity]
        if len(matches) != 1:
            return None
        name = matches[0].device
        if name not in summary["port_names"]:
            summary["port_names"].append(name)
        return name

    def open_port(name):
        connection = serial.Serial(port=None, baudrate=115200, timeout=0.05, write_timeout=2)
        connection.dtr = False
        connection.rts = False
        connection.port = name
        connection.open()
        return connection

    def save_phase(name, data):
        text = bytes(data).decode("utf-8", errors="replace")
        (args.output / f"{name}.log").write_bytes(data)
        phase_data[name] = text
        return {"received_bytes": len(data),
                "boot_markers": len(BOOT_PATTERN.findall(text)),
                "panic_markers": len(PANIC_PATTERN.findall(text))}

    # No outgoing bytes and no explicit reset during this phase.
    event("passive_start", duration_s=args.duration, target_sample_interval_s=0.25)
    data = bytearray()
    connection = None
    passive_start = time.monotonic()
    deadline = passive_start + args.duration
    next_progress = passive_start + 30
    previous_present = None
    previous_sample = None
    missing_intervals = 0
    missing_samples = 0
    samples = 0
    io_errors = 0
    max_gap = 0.0
    try:
        while time.monotonic() < deadline:
            loop_start = time.monotonic()
            if previous_sample is not None:
                max_gap = max(max_gap, loop_start - previous_sample)
            previous_sample = loop_start
            name = find_port()
            present = name is not None
            samples += 1
            if not present:
                missing_samples += 1
                if previous_present is not False:
                    missing_intervals += 1
            if previous_present != present:
                event("passive_presence", present=present, port=name)
            previous_present = present
            try:
                if connection is None and present:
                    connection = open_port(name)
                if connection is not None:
                    data.extend(connection.read(4096))
            except (serial.SerialException, OSError) as error:
                io_errors += 1
                event("passive_io_error", error=str(error))
                if connection is not None:
                    connection.close()
                    connection = None
            if time.monotonic() >= next_progress:
                event("passive_progress", measured_s=round(time.monotonic() - passive_start, 1),
                      missing_intervals=missing_intervals, io_errors=io_errors)
                next_progress += 30
            time.sleep(max(0, 0.25 - (time.monotonic() - loop_start)))
    finally:
        if connection is not None:
            connection.close()
    summary["passive"] = {
        "elapsed_s": round(time.monotonic() - passive_start, 3),
        "samples": samples, "max_sample_gap_s": round(max_gap, 3),
        "missing_samples": missing_samples, "missing_intervals": missing_intervals,
        "io_errors": io_errors, **save_phase("passive", data),
    }
    event("passive_complete", **summary["passive"])

    # Windows may still transiently change modem lines at open/close; count observations.
    event("open_close_start", requested_cycles=args.cycles)
    data = bytearray()
    completed = 0
    cycle_errors = []
    for index in range(args.cycles):
        connection = None
        try:
            name = find_port()
            if name is None:
                raise RuntimeError("Expected USB identity is absent")
            connection = open_port(name)
            until = time.monotonic() + 0.15
            while time.monotonic() < until:
                data.extend(connection.read(4096))
            connection.close()
            connection = None
            time.sleep(0.15)
            if find_port() is None:
                raise RuntimeError("USB identity absent after close")
            completed += 1
        except (serial.SerialException, OSError, RuntimeError) as error:
            record = {"cycle": index + 1, "error": str(error)}
            cycle_errors.append(record)
            event("open_close_error", **record)
            if len(cycle_errors) >= 3:
                break
        finally:
            if connection is not None:
                connection.close()
    summary["open_close"] = {
        "requested": args.cycles, "completed": completed, "errors": cycle_errors,
        **save_phase("open_close", data),
    }
    event("open_close_complete", **summary["open_close"])

    # All USB re-enumeration/reset in this phase is explicitly controlled.
    from esptool.cmds import attach_flash, read_flash
    from esptool.targets.esp32s3 import ESP32S3ROM

    event("controlled_rom_phase_start", requested_reads=args.reads)
    esp = None
    connected = False
    read_results = []
    active_errors = []
    restored_data = bytearray()
    try:
        name = find_port()
        if name is None:
            raise RuntimeError("Expected USB identity is absent before ROM test")
        esp = ESP32S3ROM(port=name, baud=115200)
        esp.connect(mode="default-reset", attempts=3)
        connected = True
        esp = esp.run_stub()
        attach_flash(esp)
        for index in range(args.reads):
            read_start = time.monotonic()
            contents = read_flash(esp, 0x10000, len(firmware), no_progress=True)
            digest = hashlib.sha256(contents).hexdigest()
            result = {"read": index + 1, "bytes": len(contents), "sha256": digest,
                      "elapsed_s": round(time.monotonic() - read_start, 3),
                      "matches": len(contents) == len(firmware) and digest == expected_hash}
            read_results.append(result)
            event("readback_complete", **result)
            if not result["matches"]:
                (args.output / f"readback_mismatch_{index + 1}.bin").write_bytes(contents)
    except Exception as error:
        active_errors.append(str(error))
        event("controlled_rom_error", error=str(error))
    finally:
        if esp is not None and connected:
            try:
                event("controlled_application_reset")
                esp.hard_reset()
                connection = esp._port
                connection.timeout = 0.1
                boot_deadline = time.monotonic() + 20
                while time.monotonic() < boot_deadline:
                    try:
                        if not connection.is_open:
                            name = find_port()
                            if name is None:
                                time.sleep(0.2)
                                continue
                            connection.port = name
                            connection.open()
                        restored_data.extend(connection.read(4096))
                    except (serial.SerialException, OSError):
                        connection.close()
                        time.sleep(0.2)
            except Exception as error:
                active_errors.append("restore: " + str(error))
                event("restore_error", error=str(error))
        if esp is not None:
            esp._port.close()

    summary["readback"] = {"requested": args.reads, "results": read_results,
                           "errors": active_errors,
                           "total_bytes": sum(r["bytes"] for r in read_results)}
    boot = save_phase("restored_boot", restored_data)
    text = phase_data["restored_boot"]
    boot["board_ok"] = "Board bring-up passed: ESP32-S3 N16R8" in text
    boot["services_started"] = "Probe services started" in text
    boot["gdb_ready"] = "GDB ready on TCP 2022" in text
    boot["confirmed"] = boot["board_ok"] and boot["services_started"] and boot["gdb_ready"]
    summary["restored_boot"] = boot
    summary["elapsed_s"] = round(time.monotonic() - start, 3)
    summary["passed"] = (
        not missing_samples and not io_errors
        and not summary["passive"]["boot_markers"] and not summary["passive"]["panic_markers"]
        and completed == args.cycles and not cycle_errors
        and not summary["open_close"]["boot_markers"] and not summary["open_close"]["panic_markers"]
        and len(read_results) == args.reads and all(r["matches"] for r in read_results)
        and not active_errors and boot["confirmed"] and not boot["panic_markers"]
    )
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    event("complete", passed=summary["passed"], elapsed_s=summary["elapsed_s"], boot=boot)
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
