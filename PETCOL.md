# petcol protocol

`petcol` is the small serial protocol the host tool (`limesplay/petcol.py`) uses
to talk to the firmware (`limesplay/limesplay_firmware/src/petprotocol.cpp`).
It frames arbitrary byte payloads, protects them with a CRC-32, and uses a
trailing sentinel byte so the receiver can find packet boundaries in a stream.

## Model and API (firmware)

A `petcol` instance does **not** own the serial link — you hand it a function
that writes raw bytes out, and it frames/deframes on top of that. There is no
"receive callback": you transmit by calling a method, and you receive by feeding
incoming bytes in one at a time and checking the return value.

```cpp
// A function that writes raw bytes to the link (e.g. Serial).
void sendfunc(const void *data, uint16_t len) {
    Serial.write((const uint8_t *)data, len);
}
// Optional: called for any byte that is NOT part of a petcol packet.
void on_extra(uint8_t b) { /* e.g. forward plain debug text */ }

petcol link(sendfunc, on_extra);   // delimiter defaults to PETCOL_BYTE (0xAA)

// --- send ---
uint8_t msg[] = { 1, 'h', 'i' };   // type 1 = LCD text
link.sendFunc(msg, sizeof(msg));   // frames CRC-32 + length + delimiter and writes it

// --- receive ---
while (Serial.available()) {
    packet_recieved *pkt = link.recv_byte_input(Serial.read());
    if (pkt) {
        // A full, CRC-validated packet arrived: pkt->data, pkt->length.
    }
}
```

- `sendFunc(data, len)` frames one packet and writes it via `sendfunc`.
- `recv_byte_input(byte)` is fed every received byte and returns a
  `packet_recieved*` once a complete, CRC-valid frame is decoded, otherwise
  `NULL`.
- `extra_data_callback` (if supplied) receives bytes that are not part of a
  packet, so a petcol channel can coexist with ordinary serial output.
- The `recvfunc` field in `pet_TL` is **not** used by this flow; it is a
  leftover and can be ignored.

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
| `0xAA`    | 1    | byte     | Frame terminator. Per-instance (see below); defaults to `PETCOL_BYTE`. |

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

## Delimiter (per instance)

The frame terminator is configurable per `petcol` instance via the constructor,
falling back to the `PETCOL_BYTE` (`0xAA`) `#define`:

```cpp
petcol stats(sendfunc);          // delimiter = 0xAA (default)
petcol debug(sendfunc, 0x00);    // separate channel, delimiter = 0x00
```

The Python client mirrors this:

```python
PetcolClient(transport, delimiter=0x00)
encode_packet(payload, delimiter=0x00)
```

Sender and receiver must agree on the delimiter. Distinct delimiters let
multiple petcol instances share one serial link as independent channels (e.g.
one for application packets and one for piped-through debug output via the
extra-data callback).

## Message types

The first payload byte selects the message:

| Type | Name      | Payload after type byte                                  |
|------|-----------|---------------------------------------------------------|
| `1`  | LCD text  | Up to 40 bytes written to the 20x2 display buffer.      |
| `2`  | LED colour| 3 bytes: red, green, blue for the "online" LED animation.|

Receiving any valid packet also resets the firmware's heartbeat timer, which
switches the display from standalone (animation) mode to online (host stats)
mode.
