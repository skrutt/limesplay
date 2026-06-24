# Qt LCD controller (petcol example)

A small Qt 5 desktop app that drives the limesplay LCD over a serial port using
**petcol**. It is a worked example of petcol on a C++ host: it both *sends*
packets (LCD text, RGB backlight) and *decodes* an incoming stream
(`recv_byte_input`), handing any non-packet bytes to a plain text console.

What it does:

- Type the two LCD lines, or mirror the title of the currently focused window
  (via `xdotool`), with optional scrolling.
- Pick the RGB backlight colour with three sliders.
- Show the board's temperature, sent back as a petcol packet.
- Print any non-packet bytes (ordinary serial output) in the console pane.

## petcol source

This example uses the repository's **canonical** petcol implementation in
[`../../petcol/`](../../petcol) — `petprotocol.cpp`, `petprotocol.h`, `buf.h` —
referenced from `petcol_test.pro` via `INCLUDEPATH`. There is no private copy to
drift out of sync with the firmware and the Python host tool.

## Build & run

Needs Qt 5 with the SerialPort module. On Debian/Ubuntu:

```sh
sudo apt-get install qtbase5-dev libqt5serialport5-dev
```

Then:

```sh
qmake && make
./petcol_test
```

The port is currently hard-coded to `/dev/ttyUSB0` at 115200 baud in
`mainwindow.cpp`; adjust it there for your board.
