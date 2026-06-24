#!/usr/bin/env python3
# coding: utf-8
"""petcol stream splitter - a worked example of the petcol host decoder.

It reads a byte stream that mixes petcol packets with ordinary traffic (e.g.
plain ``Serial.print`` debugging) and splits it live into two panes:

* **SERIAL TRAFFIC** - every byte that is *not* part of a packet, exactly as a
  normal serial console would show it.
* **PETCOL PACKETS** - each decoded, CRC-validated packet, with a timestamp,
  message type and a hex/ASCII preview.

This is the coexistence story from PETCOL.md made real: structured packets are
pulled out of the stream on the fly while everything else flows through
untouched, with nothing dropped.

Sources::

    python3 petcol_split.py --demo                 # built-in mixed stream, no HW
    python3 petcol_split.py --port /dev/ttyUSB0     # a real board

Press ``q`` (or Ctrl-C) to quit.
"""

import argparse
import curses
import os
import queue
import random
import threading
import time

from petcol import (
    MSG_LCD_TEXT,
    MSG_LED_COLOR,
    PETCOL_BYTE,
    PetcolDecoder,
    encode_packet,
)


def describe_packet(payload):
    """A short human description of a decoded packet payload."""
    if not payload:
        return "(empty)"
    msg_type = payload[0]
    body = payload[1:]
    if msg_type == MSG_LCD_TEXT:
        text = body.decode("latin-1", "replace")
        return 'type 1  LCD text   "%s"' % text
    if msg_type == MSG_LED_COLOR and len(body) >= 3:
        return "type 2  LED color  rgb(%d, %d, %d)" % (body[0], body[1], body[2])
    return "type %d  %d bytes  %s" % (msg_type, len(body), body.hex(" "))


def printable(byte):
    """Map a raw byte to something safe to show in a text pane."""
    if byte in (0x0A, 0x09):  # newline, tab
        return chr(byte)
    if 0x20 <= byte < 0x7F:
        return chr(byte)
    return "."


# --------------------------------------------------------------------------- #
# Sources: each pushes raw bytes into a queue until told to stop.
# --------------------------------------------------------------------------- #

def demo_source(out, stop):
    """Generate a realistic mixed stream: debug lines interleaved with packets.

    Packets are emitted in fragments to prove the decoder reassembles frames
    split across reads, and the debug text deliberately contains the delimiter
    byte to show it cannot fool the framing.
    """
    rng = random.Random(1)
    debug_lines = [
        b"boot: limesplay firmware ready\n",
        b"i2c: lcd 0x27 ok, max6675 ok\n",
        b"warn: heartbeat late, falling back to standalone\n",
        b"note: raw byte 0x%02X seen in stream\n" % PETCOL_BYTE,
    ]
    loop = 0
    while not stop.is_set():
        loop += 1
        # some plain debug output
        out.put(b"loop %d  temp=%.1fC  free=%dB\n"
                % (loop, 22 + rng.random() * 6, rng.randint(300, 900)))
        if loop % 3 == 0:
            out.put(rng.choice(debug_lines))

        # a couple of packets, emitted in fragments
        cpu = rng.randint(0, 99)
        packets = [
            encode_packet(bytes((MSG_LCD_TEXT,)) + ("CPU %2d%%" % cpu).encode()),
            encode_packet(bytes((MSG_LED_COLOR, rng.randint(0, 255),
                                 rng.randint(0, 255), rng.randint(0, 255)))),
        ]
        for packet in packets:
            for i in range(0, len(packet), 3):
                if stop.is_set():
                    return
                out.put(packet[i:i + 3])
                time.sleep(0.02)
        time.sleep(0.6)


def serial_source(out, stop, port, baud):
    """Read bytes from a real serial port using pyserial."""
    import serial  # imported lazily so --demo needs no dependency

    with serial.Serial(port, baud, timeout=0.1) as ser:
        while not stop.is_set():
            chunk = ser.read(256)
            if chunk:
                out.put(chunk)


# --------------------------------------------------------------------------- #
# Model: feed bytes to the decoder, keep bounded histories for the two panes.
# --------------------------------------------------------------------------- #

class SplitModel:
    def __init__(self, delimiter, max_serial=8000, max_packets=500):
        self.serial_bytes = bytearray()
        self.packets = []
        self.packet_count = 0
        self.extra_count = 0
        self._max_serial = max_serial
        self._max_packets = max_packets
        self._decoder = PetcolDecoder(
            delimiter=delimiter, on_packet=self._on_packet, on_extra=self._on_extra)

    def feed(self, data):
        self._decoder.feed(data)

    def _on_extra(self, byte):
        self.serial_bytes.append(byte)
        self.extra_count += 1
        if len(self.serial_bytes) > self._max_serial:
            del self.serial_bytes[:-self._max_serial]

    def _on_packet(self, payload):
        self.packet_count += 1
        self.packets.append((time.strftime("%H:%M:%S"), describe_packet(payload)))
        if len(self.packets) > self._max_packets:
            del self.packets[:-self._max_packets]


# --------------------------------------------------------------------------- #
# Curses UI
# --------------------------------------------------------------------------- #

def _wrap_serial(serial_bytes, width):
    """Render the serial bytes into wrapped display lines."""
    lines, line = [], ""
    for byte in serial_bytes:
        ch = printable(byte)
        if ch == "\n":
            lines.append(line)
            line = ""
            continue
        if ch == "\t":
            ch = "    "
        line += ch
        while len(line) >= width:
            lines.append(line[:width])
            line = line[width:]
    lines.append(line)
    return lines


def _draw_pane(win, x, w, top, height, title, title_attr, lines, line_attr):
    win.addnstr(top, x, title.ljust(w), w, title_attr | curses.A_BOLD)
    visible = lines[-(height - 1):] if height > 1 else []
    for i, text in enumerate(visible):
        win.addnstr(top + 1 + i, x, text.ljust(w), w, line_attr)


def run_ui(stdscr, model, byte_queue, stop, source_label):
    curses.curs_set(0)
    stdscr.nodelay(True)
    use_color = curses.has_colors()
    if use_color:
        curses.start_color()
        curses.use_default_colors()
        curses.init_pair(1, curses.COLOR_GREEN, -1)   # packets
        curses.init_pair(2, curses.COLOR_CYAN, -1)    # serial
        curses.init_pair(3, curses.COLOR_BLACK, curses.COLOR_GREEN)  # status
    green = curses.color_pair(1) if use_color else curses.A_NORMAL
    cyan = curses.color_pair(2) if use_color else curses.A_NORMAL
    status = curses.color_pair(3) if use_color else curses.A_REVERSE

    while not stop.is_set():
        # drain whatever bytes have arrived
        drained = 0
        try:
            while drained < 4096:
                model.feed(byte_queue.get_nowait())
                drained += 1
        except queue.Empty:
            pass

        height, width = stdscr.getmaxyx()
        left_w = width // 2 - 1
        right_x = width // 2 + 1
        right_w = width - right_x
        body_h = height - 1

        stdscr.erase()
        for row in range(body_h):
            stdscr.addch(row, width // 2, curses.ACS_VLINE)

        serial_lines = _wrap_serial(model.serial_bytes, max(1, left_w))
        packet_lines = ["%s  %s" % (ts, desc) for ts, desc in model.packets]

        _draw_pane(stdscr, 0, left_w, 0, body_h,
                   " SERIAL TRAFFIC (non-packet bytes) ", cyan, serial_lines, cyan)
        _draw_pane(stdscr, right_x, right_w, 0, body_h,
                   " PETCOL PACKETS ", green, packet_lines, green)

        footer = (" q quit   source: %s   packets: %d   serial bytes: %d "
                  % (source_label, model.packet_count, model.extra_count))
        stdscr.addnstr(height - 1, 0, footer.ljust(width)[:width - 1], width - 1, status)
        stdscr.refresh()

        try:
            if stdscr.getch() in (ord("q"), ord("Q")):
                stop.set()
        except curses.error:
            pass
        time.sleep(0.05)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    src = parser.add_mutually_exclusive_group(required=True)
    src.add_argument("--port", help="serial port to read, e.g. /dev/ttyUSB0")
    src.add_argument("--demo", action="store_true",
                     help="generate a built-in mixed stream (no hardware needed)")
    parser.add_argument("--baud", type=int, default=115200,
                        help="serial baud rate (default: 115200)")
    parser.add_argument("--delimiter", type=lambda v: int(v, 0), default=PETCOL_BYTE,
                        help="frame delimiter byte (default: 0xAA)")
    args = parser.parse_args(argv)

    model = SplitModel(delimiter=args.delimiter)
    byte_queue = queue.Queue()
    stop = threading.Event()

    if args.demo:
        label = "demo"
        target = lambda: demo_source(byte_queue, stop)
    else:
        label = os.path.basename(args.port)
        target = lambda: serial_source(byte_queue, stop, args.port, args.baud)

    reader = threading.Thread(target=target, daemon=True)
    reader.start()
    try:
        curses.wrapper(run_ui, model, byte_queue, stop, label)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()


if __name__ == "__main__":
    main()
