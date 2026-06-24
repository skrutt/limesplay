# limesplay
Code and firmware for a 20*2 LCD showing stats for CPU, RAM, ip adress

See firmware for hardware setup, its pretty straight forward

## Host tool (Python 3)

The `limesplay/` host tool reads CPU / RAM / clock / IP and pushes them to the
LCD over serial using the *petcol* protocol. It targets Python 3 (tested on
Manjaro).

```sh
cd limesplay
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

```
[ payload ][ checksum u32 LE ][ length u16 LE ][ 0xAA ]
```

The first payload byte is the message type (`1` = LCD text, `2` = LED RGB). The
checksum currently mirrors the firmware's additive checksum so the tool works
with existing firmware without reflashing; it is isolated in a single function
so it can later be swapped for a real CRC-32 alongside a firmware update.

### Tests

```sh
cd limesplay
python3 -m unittest discover -s tests
```

The checksum test compiles `tests/crc_reference.c` (a verbatim copy of the
firmware `make_CRC`) and asserts the Python implementation matches it.
