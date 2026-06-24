#!/usr/bin/env python3
# coding: utf-8
"""limesplay - drive a 20x2 LCD with CPU / RAM / clock / IP stats.

Rewritten for Python 3 (tested on Arch/Manjaro). Talks to the limesplay
firmware over serial using the petcol protocol (see ``petcol.py``).

Examples::

    limesplay.py                      # auto-detect port, run forever
    limesplay.py --port /dev/ttyUSB0  # explicit port
    limesplay.py --interface wlan0    # prefer a specific NIC for the IP line
    limesplay.py --no-serial          # render to stdout, no hardware needed
"""

import argparse
import glob
import sys
import time
from datetime import datetime

import psutil

from petcol import PetcolClient

LCD_COLS = 20
LCD_ROWS = 2
LCD_SIZE = LCD_COLS * LCD_ROWS

FULL_BLOCK = "\xff"  # HD44780 solid block glyph

# Interfaces tried (in order) when none is given on the command line.
DEFAULT_IFACE_ORDER = ("wlan0", "wlan1", "eth0", "eth1", "eth2", "eth3", "usb0")

# HD44780 (A02 ROM) codes for Scandinavian glyphs, kept from the original tool.
SCANDINAVIAN_MAP = {
    "å": 0x86, "Å": 0x8F,
    "ä": 0xE1, "Ä": 0x8E,
    "ö": 0xEF, "Ö": 0x99,
}


def to_lcd_bytes(text):
    """Encode ``text`` for the LCD, remapping Scandinavian glyphs."""
    out = bytearray()
    for ch in text:
        if ch in SCANDINAVIAN_MAP:
            out.append(SCANDINAVIAN_MAP[ch])
        else:
            out.extend(ch.encode("latin-1", "replace"))
    return bytes(out)


def make_bar(percent, length=12):
    """Render a ``[####    ]`` style bar of total width ``length``."""
    inner = length - 2
    filled = int(round(percent / 100.0 * inner))
    filled = max(0, min(inner, filled))
    return "[" + FULL_BLOCK * filled + " " * (inner - filled) + "]"


def fit(text, width=LCD_COLS):
    """Pad or truncate ``text`` to exactly ``width`` characters."""
    return text[:width].ljust(width)


def get_ip(preferred=None):
    """Return the first non-loopback IPv4 address, honouring ``preferred``."""
    addrs = psutil.net_if_addrs()
    order = []
    if preferred:
        order.append(preferred)
    order.extend(i for i in DEFAULT_IFACE_ORDER if i != preferred)
    # Fall back to whatever else exists on the machine.
    order.extend(i for i in addrs if i not in order)

    for iface in order:
        for snic in addrs.get(iface, ()):
            if snic.family == psutil.AF_INET and not snic.address.startswith("127."):
                return snic.address
    return "no_ip"


def render(count, preferred_iface=None):
    """Build the 40-character LCD buffer for the current tick."""
    now = datetime.now()
    cpu = psutil.cpu_percent(interval=None)
    ram = psutil.virtual_memory().percent

    line1 = fit("CPU" + make_bar(cpu) + now.strftime("%H:%M"))

    # Show the IP for 5 out of every 15 ticks, like the original.
    if (count % 15) >= 10:
        line2 = fit("IP " + get_ip(preferred_iface))
    else:
        line2 = fit("RAM" + make_bar(ram) + now.strftime("%m-%d"))

    return line1 + line2


def find_port():
    """Best-effort auto-detection of the serial port."""
    try:
        from serial.tools import list_ports
        ports = [p.device for p in list_ports.comports()]
    except Exception:
        ports = []
    ports += sorted(glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*"))
    seen = []
    for p in ports:
        if p not in seen:
            seen.append(p)
    return seen[0] if seen else None


class StdoutTransport:
    """Fake transport for ``--no-serial``: renders the latest LCD text."""

    def write(self, data):
        # Strip the petcol framing back off so we can show what the LCD sees.
        # Payload is everything before the 7-byte trailer (u32 + u16 + 0xAA).
        payload = data[:-7]
        if not payload or payload[0] != 1:
            return  # only LCD-text packets are human-readable
        text = payload[1:].decode("latin-1")
        top, bottom = text[:LCD_COLS], text[LCD_COLS:LCD_SIZE]
        bar = "+" + "-" * LCD_COLS + "+"
        sys.stdout.write("\r\n".join((bar, "|" + fit(top) + "|",
                                      "|" + fit(bottom) + "|", bar)) + "\r\n\n")
        sys.stdout.flush()


def open_serial(port, baud):
    import serial
    if port is None:
        port = find_port()
        if port is None:
            sys.exit("No serial port found. Pass --port or use --no-serial.")
    print("Opening %s @ %d baud" % (port, baud))
    return serial.Serial(port, baud, timeout=0)


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", help="serial device (default: auto-detect)")
    parser.add_argument("--baud", type=int, default=115200,
                        help="baud rate (default: 115200, matches firmware)")
    parser.add_argument("--interface", help="preferred network interface for the IP line")
    parser.add_argument("--interval", type=float, default=0.3,
                        help="seconds between updates (default: 0.3)")
    parser.add_argument("--led", metavar="R,G,B",
                        help="set the online LED colour once at startup, e.g. 255,0,255")
    parser.add_argument("--no-serial", action="store_true",
                        help="render to stdout instead of a serial port")
    parser.add_argument("--once", action="store_true",
                        help="render a single frame and exit")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)

    if args.no_serial:
        transport = StdoutTransport()
    else:
        transport = open_serial(args.port, args.baud)

    client = PetcolClient(transport)

    if args.led:
        try:
            r, g, b = (int(x) for x in args.led.split(","))
        except ValueError:
            sys.exit("--led expects R,G,B integers, e.g. 255,0,255")
        client.send_color(r, g, b)

    psutil.cpu_percent(interval=None)  # prime the CPU meter

    count = 0
    try:
        while True:
            client.send_text(to_lcd_bytes(render(count, args.interface)))
            if args.once:
                break
            count = (count + 1) % 256
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nbye")


if __name__ == "__main__":
    main()
