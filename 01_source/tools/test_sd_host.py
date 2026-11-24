#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build/run production microSD fault-injection tests without a card or ESP32.

The SDK and VFS stubs are test doubles; no real filesystem/card is accessed by
the C executable. This checks component logic, not ESP-IDF or electrical timing.
"""
import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    """Compile and run enabled/disabled SD host scenarios with the selected C compiler.

    Generated executables stay under .cache/sd_tests. Compiler or assertion failures
    propagate through subprocess.run; no serial device or physical card is opened.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="gcc", help="Host GCC compiler path")
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if not compiler:
        parser.error(f"host compiler not found: {args.cc}")
    source = Path(__file__).resolve().parents[1]
    output = source / ".cache" / "sd_tests"
    output.mkdir(parents=True, exist_ok=True)
    for enabled in (1, 0):
        executable = output / f"sd_test_{enabled}.exe"
        command = [
            compiler, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
            "-Wconversion", "-Wsign-conversion", "-Wno-unused-function",
            "-Wno-unused-variable", f"-DCONFIG_PROBE_SD_ENABLE={enabled}",
            "-I", str(source / "tests" / "sd_stubs"),
            "-I", str(source / "components" / "probe_sd" / "include"),
            str(source / "tests" / "sd_test.c"), "-o", str(executable),
        ]
        subprocess.run(command, check=True)
        subprocess.run([str(executable)], check=True)
    print("PASS: enabled/disabled microSD host test builds")


if __name__ == "__main__":
    main()
