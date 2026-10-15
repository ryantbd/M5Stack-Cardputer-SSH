#!/usr/bin/env python3
"""Send a setup command to Cardputer SSH over USB serial.

    python3 tools/cardputer_serial.py keys
    python3 tools/cardputer_serial.py vpn-import wg0.conf
    python3 tools/cardputer_serial.py host-add host.txt
    printf 'ssid=%s\\npassword=%s\\n' "$SSID" "$PASS" | python3 tools/cardputer_serial.py wifi-add -

The body (key=value lines, or a wg-quick config for vpn-import) is read from
a file, or from stdin when given as "-". Only the device's reply is printed,
never the body. The port opens with DTR high and RTS low so the ESP32-S3's
USB Serial/JTAG does not reset the device. Requires pyserial.
"""

import argparse
import glob
import sys
import time

import serial

COMMANDS = ("keys", "wifi-add", "host-add", "vpn-import")


def find_port():
    ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))
    if len(ports) != 1:
        sys.exit(f"Found {len(ports) or 'no'} candidate ports; pass --port explicitly.")
    return ports[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("command", choices=COMMANDS)
    parser.add_argument("body", nargs="?", help='file with the command body, or "-" for stdin')
    parser.add_argument("--port", help="serial port (default: the only USB modem port)")
    args = parser.parse_args()
    if args.command != "keys" and args.body is None:
        parser.error(f"{args.command} needs a body file or -")

    lines = []
    if args.body is not None:
        source = sys.stdin if args.body == "-" else open(args.body, encoding="utf-8")
        with source:
            lines = [line.rstrip("\r\n") for line in source]

    link = serial.Serial()
    link.port, link.baudrate, link.timeout = args.port or find_port(), 115200, 0.2
    link.dtr, link.rts = True, False
    link.open()
    time.sleep(0.3)
    link.reset_input_buffer()
    for line in [args.command] + (lines + ["END"] if args.body is not None else []):
        link.write((line + "\n").encode())
        link.flush()
        time.sleep(0.02)

    expected = ("ssh-public-key:", "wireguard-public-key:") if args.command == "keys" else (args.command + ":",)
    reply = b""
    deadline = time.time() + 6
    while time.time() < deadline:
        try:
            reply += link.read(512)
        except serial.SerialException:
            break  # the device restarted to apply the change
        if expected[-1].encode() in reply and reply.endswith(b"\n"):
            break
    link.close()

    replies = [line for line in reply.decode(errors="replace").splitlines()
               if line.startswith(expected + ("error:",))]
    if not replies:
        sys.exit("No reply. Is Cardputer SSH running and connected over USB?")
    print("\n".join(replies))
    sys.exit(1 if any("error:" in line for line in replies) else 0)


if __name__ == "__main__":
    main()
