#!/usr/bin/env python3
"""NX01 Nixie Watch - Time Sync Tool"""

import sys
import time
import platform

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("pyserial is required: pip install pyserial")
    sys.exit(1)

TIMEZONES = [
    ("HST",  -10, "Hawaii"),
    ("AKST",  -9, "Alaska Standard"),
    ("AKDT",  -8, "Alaska Daylight"),
    ("PST",   -8, "Pacific Standard"),
    ("PDT",   -7, "Pacific Daylight"),
    ("MST",   -7, "Mountain Standard"),
    ("MDT",   -6, "Mountain Daylight"),
    ("CST",   -6, "Central Standard"),
    ("CDT",   -5, "Central Daylight"),
    ("EST",   -5, "Eastern Standard"),
    ("EDT",   -4, "Eastern Daylight"),
    ("AST",   -4, "Atlantic Standard"),
    ("ADT",   -3, "Atlantic Daylight"),
    ("UTC",    0, "UTC / GMT"),
    ("CET",   +1, "Central European"),
    ("CEST",  +2, "Central European Summer"),
    ("JST",   +9, "Japan Standard"),
    ("AEST", +10, "Australian Eastern Standard"),
    ("AEDT", +11, "Australian Eastern Daylight"),
]


def detect_ports():
    return [p.device for p in serial.tools.list_ports.comports()]


def get_system_offset():
    if time.daylight and time.localtime().tm_isdst:
        return -time.altzone / 3600
    return -time.timezone / 3600


def pick_port(ports):
    if not ports:
        print("No serial ports found.")
        print("Make sure the watch is connected and in sync mode (flashing 9-6).")
        sys.exit(1)

    if len(ports) == 1:
        print(f"Found port: {ports[0]}")
        return ports[0]

    print("Available ports:")
    for i, p in enumerate(ports, 1):
        print(f"  {i}. {p}")
    while True:
        try:
            choice = int(input("\nSelect port: "))
            if 1 <= choice <= len(ports):
                return ports[choice - 1]
        except (ValueError, EOFError):
            pass
        print("Invalid selection.")


def pick_timezone():
    system_offset = get_system_offset()
    matches = [tz for tz in TIMEZONES if tz[1] == system_offset]
    match_names = "/".join(m[0] for m in matches)

    print(f"\nDetected system timezone: UTC{system_offset:+.0f}", end="")
    if match_names:
        print(f" ({match_names})")
    else:
        print()

    print("\n  1. Use system timezone")
    print("  2. Choose from list")
    print("  3. Enter UTC offset manually")

    while True:
        try:
            choice = input("\nSelect [1]: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nCancelled.")
            sys.exit(0)

        if choice in ("", "1"):
            return system_offset

        if choice == "2":
            print()
            for i, (name, off, desc) in enumerate(TIMEZONES, 1):
                print(f"  {i:2d}. {name:4s}  UTC{off:+d}  ({desc})")
            while True:
                try:
                    tz_choice = int(input("\nSelect timezone: "))
                    if 1 <= tz_choice <= len(TIMEZONES):
                        name, off, desc = TIMEZONES[tz_choice - 1]
                        print(f"Selected: {name} (UTC{off:+d})")
                        return off
                except (ValueError, EOFError):
                    pass
                print("Invalid selection.")

        if choice == "3":
            while True:
                try:
                    return float(input("UTC offset (e.g. -4, +5.5): "))
                except (ValueError, EOFError):
                    print("Invalid offset.")

        print("Invalid selection.")


def sync(port, offset_hours):
    utc_epoch = int(time.time())
    local_epoch = utc_epoch + int(offset_hours * 3600)

    print(f"\n  UTC:   {time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(utc_epoch))}")
    print(f"  Local: {time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(local_epoch))}")
    print(f"  Offset: UTC{offset_hours:+.0f}")

    try:
        s = serial.Serial(port, 9600, timeout=2)
        time.sleep(0.1)
        s.reset_input_buffer()
        s.write(f"{local_epoch}\n".encode())
        s.flush()
        time.sleep(0.5)
        s.close()
        print("\nSync complete. Watch for the 9-0 sweep on the tube.")
    except serial.SerialException as e:
        print(f"\nError: {e}")
        print("Make sure the watch is in sync mode and the port is correct.")
        sys.exit(1)


def main():
    print("=" * 40)
    print("  NX01 Nixie Watch - Time Sync")
    print("=" * 40)

    port = pick_port(detect_ports())
    offset = pick_timezone()
    sync(port, offset)


if __name__ == "__main__":
    main()
