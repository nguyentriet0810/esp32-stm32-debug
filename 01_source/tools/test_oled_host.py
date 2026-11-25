#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the production settings menu and SH1106 transfers against a host RAM model.

Checks protocol, bounds and fault cleanup; it does not verify real I2C waveforms or glass mapping.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess


def main():
    """Build/run disabled OLED and offsets 0/2/4 using the selected host C compiler.

    Executables are generated under .cache/oled_tests. Compiler and assertion failures
    propagate to the caller; no physical display or serial connection is opened.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="gcc", help="Host C compiler executable")
    parser.add_argument("--preview-dir", type=Path, help="Save startup/menu PGM frames for review")
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if not compiler:
        parser.error(f"host compiler not found: {args.cc}")
    source = Path(__file__).resolve().parents[1]
    output = source / ".cache" / "oled_tests"
    output.mkdir(parents=True, exist_ok=True)
    if args.preview_dir:
        args.preview_dir.mkdir(parents=True, exist_ok=True)
    for enabled, offset in ((1, 0), (1, 2), (1, 4), (0, 2)):
        executable = output / f"oled_test_{enabled}_{offset}.exe"
        command = [
            compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wconversion",
            "-Wsign-conversion", "-Wstrict-prototypes", "-Wmissing-prototypes",
            f"-DCONFIG_PROBE_OLED_ENABLE={enabled}",
            f"-DCONFIG_PROBE_OLED_COLUMN_OFFSET={offset}",
            "-I", str(source / "tests" / "oled_stubs"),
            "-I", str(source / "components" / "probe_oled" / "include"),
            "-I", str(source / "components" / "probe_local_ui"),
            str(source / "components" / "probe_oled" / "probe_oled.c"),
            str(source / "components" / "probe_local_ui" / "probe_ui_settings.c"),
            str(source / "tests" / "oled_test.c"), "-o", str(executable),
        ]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment.pop("PROBE_OLED_PREVIEW_DIR", None)
        if args.preview_dir and enabled and offset == 2:
            environment["PROBE_OLED_PREVIEW_DIR"] = str(args.preview_dir.resolve())
        subprocess.run([str(executable)], check=True, env=environment)
    print("PASS: SH1106 host tests for offsets 0/2/4 and disabled OLED")


if __name__ == "__main__":
    main()
