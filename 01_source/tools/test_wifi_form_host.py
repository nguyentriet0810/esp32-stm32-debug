#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile and run the bounded Wi-Fi form codec tests without network or board access."""
import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    """Build production codec with strict host warnings and propagate assertion failures."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="gcc")
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if not compiler:
        parser.error("host C compiler not found")
    root = Path(__file__).resolve().parents[1]
    component = root / "components" / "probe_wifi_setup"
    output = root / ".cache" / "wifi_form_tests"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "wifi_form_test.exe"
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wconversion",
                    "-Wsign-conversion", "-Wstrict-prototypes", "-Wmissing-prototypes",
                    "-I", str(component), str(component / "probe_wifi_form.c"),
                    str(root / "tests" / "wifi_form_test.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
