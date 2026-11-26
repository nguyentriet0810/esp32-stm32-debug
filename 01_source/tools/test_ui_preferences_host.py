#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build/run real OLED preference/model code with isolated NVS mocks and no board access."""
import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    """Compile with strict host warnings and propagate failed persistence assertions."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='gcc')
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if not compiler:
        parser.error('host C compiler not found')
    root = Path(__file__).resolve().parents[1]
    component = root / 'components/probe_local_ui'
    output = root / '.cache/ui_preferences_tests'
    output.mkdir(parents=True, exist_ok=True)
    executable = output / 'ui_preferences_test.exe'
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wconversion',
                    '-Wsign-conversion', '-Wstrict-prototypes', '-Wmissing-prototypes',
                    '-I', str(root / 'tests/oled_stubs'), '-I', str(component),
                    str(component / 'probe_ui_settings.c'), str(component / 'probe_ui_preferences.c'),
                    str(root / 'tests/ui_preferences_test.c'), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    main()
