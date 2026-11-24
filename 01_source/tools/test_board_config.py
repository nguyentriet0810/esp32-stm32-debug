#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check PCB GPIOs and firmware defaults against an exported KiCad XML netlist.

This is an offline configuration check, not a runtime hardware test. Export the
current schematic with kicad-cli before running; old netlists can hide changes.
"""

import argparse
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET


SIGNALS = (
    ("PROBE_SWDIO_GPIO", "SWDIO", 4),
    ("PROBE_SWCLK_GPIO", "SWCLK", 5),
    ("PROBE_SWO_GPIO", "SWO", 6),
    ("PROBE_NRST_GPIO", "TARGET_NRST", 7),
    ("PROBE_OLED_SDA_GPIO", "OLED_SDA", 8),
    ("PROBE_OLED_SCL_GPIO", "OLED_SCL", 9),
    ("PROBE_ENCODER_A_GPIO", "ENC_A", 15),
    ("PROBE_ENCODER_B_GPIO", "ENC_B", 16),
    ("PROBE_ENCODER_SW_GPIO", "ENC_SW", 17),
    ("PROBE_SD_CS_GPIO", "SD_CS_N", 38),
    ("PROBE_SD_MOSI_GPIO", "SD_MOSI", 39),
    ("PROBE_SD_SCK_GPIO", "SD_SCK", 40),
    ("PROBE_SD_MISO_GPIO", "SD_MISO", 41),
    ("PROBE_SD_DETECT_GPIO", "SD_DETECT_N", 42),
)
OPTIONAL_PERIPHERALS = ("PROBE_OLED_ENABLE", "PROBE_ENCODER_ENABLE", "PROBE_SD_ENABLE")


def read_kconfig_defaults(path):
    """Return first unconditional scalar defaults indexed by Kconfig symbol.

    Args:
        path: UTF-8 board Kconfig path.
    Returns:
        A new symbol-to-text dictionary; the source file is not modified.
    """
    result = {}
    symbol = None
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"config\s+(\w+)\s*", line)
        if match:
            symbol = match[1]
        elif symbol:
            match = re.fullmatch(r"\s+default\s+(\S+)\s*", line)
            if match:
                result.setdefault(symbol, match[1])
    return result


def read_sdkconfig(path):
    """Parse SDK configuration values, including disabled-bool comment syntax.

    Args:
        path: UTF-8 defaults or generated sdkconfig file.
    Returns:
        Symbol values as strings, with disabled bools represented by "n".
    """
    result = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"CONFIG_(\w+)=(.*)", line)
        if match:
            result[match[1]] = match[2]
        else:
            match = re.fullmatch(r"# CONFIG_(\w+) is not set", line)
            if match:
                result[match[1]] = "n"
    return result


def check_netlist(path, errors):
    """Append schematic pin and connector mismatches to the caller-owned errors list.

    Args:
        path: Freshly exported KiCad XML netlist.
        errors: Mutable diagnostic list updated in place.
    Raises:
        OSError: The netlist cannot be read.
        ET.ParseError: The input is not valid XML.
    """
    tree = ET.parse(path)
    nets = {}
    pins = {}
    for net in tree.findall("./nets/net"):
        name = net.get("name", "").lstrip("/")
        nets.setdefault(name, []).append(net)
        for node in net.findall("node"):
            key = (node.get("ref"), node.get("pin"))
            pins.setdefault(key, []).append(name)

    for _, signal, expected_gpio in SIGNALS:
        nodes = [
            node
            for net in nets.get(signal, [])
            for node in net.findall("node")
            if node.get("ref") == "U1"
        ]
        actual = [node.get("pinfunction", "") for node in nodes]
        if len(nodes) != 1 or re.fullmatch(
            rf"IO{expected_gpio}(?:_\d+)?", actual[0] if actual else ""
        ) is None:
            errors.append(
                f"{path}: /{signal} must connect to U1 GPIO{expected_gpio}; "
                f"found U1 pin functions {actual or 'none'}"
            )

    def expect_pin(reference, pin, expected_net):
        """Append a diagnostic unless one connector pin has exactly the expected net."""
        actual = pins.get((reference, pin), [])
        if actual != [expected_net]:
            errors.append(
                f"{path}: {reference}.{pin} must be on /{expected_net}; "
                f"found {actual or 'no connection'}"
            )

    for pin, net in (("1", "GND"), ("2", "+3V3"), ("3", "OLED_SCL"), ("4", "OLED_SDA")):
        expect_pin("J3", pin, net)
    expect_pin("J4", "1", "GND")
    for header_pin, resistor, mcu_net, external_net in (
        ("2", "R17", "SWDIO", "EXT_SWDIO"),
        ("3", "R18", "SWCLK", "EXT_SWCLK"),
        ("4", "R19", "SWO", "EXT_SWO"),
        ("5", "R20", "TARGET_NRST", "EXT_NRST"),
    ):
        expect_pin("J4", header_pin, external_net)
        expect_pin(resistor, "1", external_net)
        expect_pin(resistor, "2", mcu_net)
    for pin, net in (
        ("2", "SD_CS_N"), ("3", "SD_MOSI"), ("4", "SD_3V3"),
        ("5", "SD_CLK"), ("6", "GND"), ("7", "SD_MISO"),
        ("9", "SD_DETECT_N"), ("SH", "GND"),
    ):
        expect_pin("J5", pin, net)
    expect_pin("R34", "1", "SD_SCK")
    expect_pin("R34", "2", "SD_CLK")
    expect_pin("R41", "1", "SD_3V3")
    expect_pin("R41", "2", "SD_DETECT_N")


def check_config(values, label, errors, generated=False):
    """Append PCB configuration mismatches without altering the supplied values.

    Args:
        values: Symbol-to-text configuration dictionary.
        label: Source label included in diagnostics.
        errors: Caller-owned list to which mismatches are appended.
        generated: Permit explicit enable/disable choices for optional peripherals.
    """
    expected = {symbol: str(gpio) for symbol, _, gpio in SIGNALS}
    if not generated:
        expected.update({symbol: "y" for symbol in OPTIONAL_PERIPHERALS})
    expected["PROBE_BOARD_PCB_REV_C"] = "y"
    for symbol, value in expected.items():
        actual = values.get(symbol)
        if actual != value:
            errors.append(
                f"{label}: CONFIG_{symbol} must be {value}; "
                f"found {actual if actual is not None else 'missing'}"
            )
    if generated:
        for symbol in OPTIONAL_PERIPHERALS:
            actual = values.get(symbol)
            if actual not in ("y", "n"):
                errors.append(
                    f"{label}: CONFIG_{symbol} must be y or n; "
                    f"found {actual if actual is not None else 'missing'}"
                )


def main():
    """Compare schematic, defaults and optional generated configuration from CLI arguments.

    Returns:
        Zero on agreement, or one after printing accumulated diagnostics.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--netlist", type=Path, required=True, help="Fresh KiCad XML netlist")
    parser.add_argument("--sdkconfig", type=Path, help="Also check the generated build configuration")
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    kconfig_path = source / "components" / "probe_board" / "Kconfig"
    defaults_path = source / "sdkconfig.defaults"
    errors = []
    try:
        check_netlist(args.netlist, errors)
        kconfig = read_kconfig_defaults(kconfig_path)
        check_config(kconfig, kconfig_path, errors)
        defaults = {**kconfig, **read_sdkconfig(defaults_path)}
        check_config(defaults, f"{defaults_path} (with Kconfig defaults)", errors)
        if args.sdkconfig:
            check_config(read_sdkconfig(args.sdkconfig), args.sdkconfig, errors, generated=True)
    except (OSError, ET.ParseError) as error:
        errors.append(str(error))
    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print("PASS: 14 schematic GPIOs, OLED/debug/SD connectors and firmware PCB defaults agree")
    if args.sdkconfig:
        print(f"PASS: generated configuration {args.sdkconfig} matches the PCB")
    print("Offline configuration check only; runtime hardware is not tested.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
