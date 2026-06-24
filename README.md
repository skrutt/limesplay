# limesplay
Code and firmware for a 20*2 LCD showing stats for CPU, RAM, ip adress

See firmware for hardware setup, its pretty straight forward

## petcol — the protocol behind it

The host and firmware talk over **petcol**: a tiny self-synchronising framing
layer that wraps arbitrary payloads in a CRC-32 and a trailing delimiter. The
point is that it *gets out of your way* — structured packets and ordinary
`Serial.print` debugging can share a single link, and a stray delimiter inside
your data can't break framing because the CRC has to match too. Full spec in
[PETCOL.md](PETCOL.md).

![petcol: one wire carries framed packets and plain debug text; the receiver splits them apart](docs/petcol-coexist.svg)

## Repository layout

```
host/                  Python 3 host tool (sends stats to the LCD over serial)
limesplay_firmware/    Arduino sketch (open this folder in the Arduino IDE/CLI)
PETCOL.md              petcol protocol specification
```

## Host tool (Python 3)

The `host/` tool reads CPU / RAM / clock / IP and pushes them to the LCD over
serial using the *petcol* protocol. It targets Python 3 (tested on Manjaro).

```sh
cd host
python3 -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt

python3 limesplay.py                      # auto-detect port, run forever
python3 limesplay.py --port /dev/ttyUSB0  # explicit port
python3 limesplay.py --interface wlan0    # prefer a NIC for the IP line
python3 limesplay.py --led 255,0,255      # set the online LED colour
python3 limesplay.py --no-serial          # render to stdout, no hardware needed
```

Run `python3 limesplay.py --help` for all options.

### Protocol

`petcol.py` implements the serial framing the firmware expects:

![petcol frame layout: payload, crc32, length, delimiter](docs/petcol-frame.svg)

The first payload byte is the message type (`1` = LCD text, `2` = LED RGB) and
the checksum is a standard CRC-32, matching the firmware's `make_CRC`. See
[PETCOL.md](PETCOL.md) for the full protocol spec.

## Firmware (Arduino)

The firmware is a self-contained Arduino sketch in `limesplay_firmware/`. Open
that folder (its `.ino` matches the folder name, as Arduino requires) in the
Arduino IDE or build it with `arduino-cli compile`. Adafruit GFX, LiquidCrystal,
MAX6675 and Vector are pulled from your installed libraries.

### Tests

```sh
cd host
python3 -m unittest discover -s tests
```

The checksum test compiles `tests/crc_reference.c` (a verbatim copy of the
firmware `make_CRC`) and asserts the Python implementation matches it.
