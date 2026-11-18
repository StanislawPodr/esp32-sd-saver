#!/usr/bin/env python3
"""CLI for the ESP32 variable store exposed over UART.

The firmware understands newline terminated commands::

    get <name>
    set <name> <int|bool|float> <value>
    del <name>

Only ``get`` sends a reply. All other serial output (ESP-IDF logs) is hidden
by default; use ``--verbose`` to see it.
"""

from __future__ import annotations

import argparse
import re
import sys
import time
from typing import Iterator

import serial

DEFAULT_PORT = "/dev/ttyUSB0"
DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT = 2.0

NAME_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]{0,19}\Z")
LOG_RE = re.compile(r"[IWEDV] ?\(\d+\)")
VALUE_RE = re.compile(r"([A-Za-z0-9_]+)\s+(int|bool|float)\s+(\S.*?)\s*\Z")


def variable_name(value: str) -> str:
    """Validate names accepted by the firmware's sscanf pattern."""
    if not NAME_RE.fullmatch(value):
        raise argparse.ArgumentTypeError(
            "name must be 1-20 characters, start with a letter or '_' "
            "and contain only letters, digits and '_'"
        )
    return value


def int_value(value: str) -> str:
    try:
        return str(int(value))
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid int value: {value!r}") from exc


def bool_value(value: str) -> str:
    lowered = value.lower()
    if lowered in {"1", "true", "yes", "on"}:
        return "1"
    if lowered in {"0", "false", "no", "off"}:
        return "0"
    raise argparse.ArgumentTypeError(
        "bool value must be 0/1, true/false, yes/no or on/off"
    )


def float_value(value: str) -> str:
    try:
        float(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid float value: {value!r}") from exc
    return value


def format_value(value_type: str, raw_value: str) -> str:
    if value_type == "int":
        return int_value(raw_value)
    if value_type == "bool":
        return bool_value(raw_value)
    return float_value(raw_value)


def add_common_options(target: argparse.ArgumentParser) -> None:
    """Add connection options to a parser.

    Defaults are suppressed on subparsers so an option given before the
    command is not overwritten by one that was not given after it.
    """
    target.add_argument(
        "-p", "--port", default=argparse.SUPPRESS,
        help=f"serial port (default: {DEFAULT_PORT})",
    )
    target.add_argument(
        "-b", "--baud", type=int, default=argparse.SUPPRESS,
        help=f"baud rate (default: {DEFAULT_BAUD})",
    )
    target.add_argument(
        "-t", "--timeout", type=float, default=argparse.SUPPRESS,
        help=f"seconds to wait for a reply (default: {DEFAULT_TIMEOUT})",
    )
    target.add_argument(
        "-v", "--verbose", action="store_true", default=argparse.SUPPRESS,
        help="print all UART traffic, including ESP-IDF logs",
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Read and write ESP32 variables over UART.",
    )
    add_common_options(parser)
    parser.set_defaults(
        port=DEFAULT_PORT,
        baud=DEFAULT_BAUD,
        timeout=DEFAULT_TIMEOUT,
        verbose=False,
    )

    commands = parser.add_subparsers(
        dest="command", required=True, metavar="{get,set,del}",
    )

    get_parser = commands.add_parser("get", help="read a variable")
    add_common_options(get_parser)
    get_parser.add_argument("name", type=variable_name)

    set_parser = commands.add_parser("set", help="create or overwrite a variable")
    add_common_options(set_parser)
    set_parser.add_argument("name", type=variable_name)
    set_parser.add_argument("type", choices=("int", "bool", "float"))
    set_parser.add_argument("value", help="value matching the chosen type")

    del_parser = commands.add_parser("del", help="delete a variable")
    add_common_options(del_parser)
    del_parser.add_argument("name", type=variable_name)

    return parser


def build_command(args: argparse.Namespace) -> str:
    if args.command == "get":
        return f"get {args.name}"
    if args.command == "del":
        return f"del {args.name}"
    return f"set {args.name} {args.type} {format_value(args.type, args.value)}"


def read_lines(ser: serial.Serial, seconds: float) -> Iterator[str]:
    """Yield complete lines received within ``seconds``."""
    deadline = time.monotonic() + seconds
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return
        ser.timeout = remaining
        raw = ser.readline()
        if not raw:
            return
        yield raw.decode("utf-8", errors="replace").rstrip("\r\n")


def print_result(line: str) -> bool:
    """Print a firmware reply line; return True if it was a reply."""
    match = VALUE_RE.fullmatch(line.strip())
    if match is None:
        return False
    print(f"{match.group(1)} {match.group(2)} {match.group(3)}")
    return True


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        command = build_command(args)
    except argparse.ArgumentTypeError as exc:
        parser.error(str(exc))

    try:
        with serial.Serial(
            port=args.port,
            baudrate=args.baud,
            timeout=args.timeout,
            write_timeout=args.timeout,
        ) as ser:
            ser.reset_input_buffer()
            ser.write(command.encode() + b"\n")
            ser.flush()

            if args.verbose:
                for line in read_lines(ser, args.timeout):
                    print(line)
            elif args.command == "get":
                for line in read_lines(ser, args.timeout):
                    if LOG_RE.match(line):
                        continue
                    if print_result(line):
                        break
            else:
                # set/del have no reply; drain their log output silently.
                for _ in read_lines(ser, 0.1):
                    pass
    except (serial.SerialException, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
