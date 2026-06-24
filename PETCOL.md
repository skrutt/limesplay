# petcol protocol

`petcol` is the small serial protocol the host tool (`limesplay/petcol.py`) uses
to talk to the firmware (`limesplay/limesplay_firmware/src/petprotocol.cpp`).
It frames arbitrary byte payloads, protects them with a CRC-32, and uses a
trailing sentinel byte so the receiver can find packet boundaries in a stream.

## Frame layout

A packet is sent as:

```
+-----------------+--------------+--------------+-----------+
| payload (N)     | crc32 (u32)  | length (u16) | 0xAA      |
+-----------------+--------------+--------------+-----------+
  N bytes           4 bytes LE     2 bytes LE     1 byte
```

| Field     | Size | Encoding | Notes                                            |
|-----------|------|----------|--------------------------------------------------|
| `payload` | N    | raw      | Application bytes. `payload[0]` is the message type. |
| `crc32`   | 4    | u32 LE   | CRC-32 of the `payload` bytes (see below).        |
| `length`  | 2    | u16 LE   | `N`, the payload length.                           |
| `0xAA`    | 1    | byte     | `PETCOL_BYTE` frame terminator.                   |

`N` must satisfy `1 <= N < PACKETSIZE_MAX` (128). The header struct on the
firmware (`packet_header { uint32_t CRC; uint16_t length; }`) is 6 bytes on AVR
(1-byte alignment), so the on-wire trailer after the payload is exactly
`4 + 2 + 1 = 7` bytes.

## CRC-32

Standard CRC-32 (IEEE 802.3 / zlib / PNG):

- reflected input and output,
- polynomial `0xEDB88320`,
- initial value `0xFFFFFFFF`,
- final XOR `0xFFFFFFFF`,
- computed over the `payload` bytes only (not the header).

Check value: CRC-32 of the ASCII string `"123456789"` is `0xCBF43926`.

The firmware implements it bitwise in `petcol::make_CRC`; the host uses
`zlib.crc32`. Both are byte-for-byte identical, and the test
`limesplay/tests/test_petcol.py` compiles a verbatim copy of the firmware
routine (`crc_reference.c`) and asserts the two agree.

## Receiving

The firmware feeds every incoming byte to `recv_byte_input()`, which appends to
a ring buffer. When it sees `0xAA` and enough bytes are buffered, it reads the
`length`, sanity-checks it (`length < PACKETSIZE_MAX` and enough bytes present),
reads the stored `crc32` and the payload, recomputes the CRC over the payload,
and only accepts the packet if the two match. On a mismatch the bytes are
restored to the buffer and scanning continues, so a stray `0xAA` inside data
does not corrupt framing.

## Message types

The first payload byte selects the message:

| Type | Name      | Payload after type byte                                  |
|------|-----------|---------------------------------------------------------|
| `1`  | LCD text  | Up to 40 bytes written to the 20x2 display buffer.      |
| `2`  | LED colour| 3 bytes: red, green, blue for the "online" LED animation.|

Receiving any valid packet also resets the firmware's heartbeat timer, which
switches the display from standalone (animation) mode to online (host stats)
mode.
