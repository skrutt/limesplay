# coding: utf-8
"""Python client for the *petcol* serial protocol used by the limesplay firmware.

Wire format (as implemented by ``petprotocol.cpp`` in the firmware)::

    [ payload bytes ][ checksum u32 LE ][ length u16 LE ][ 0xAA ]

* ``payload``  - the message bytes. The first byte is the message *type*.
* ``checksum`` - 32-bit value computed by :func:`petcol_checksum` over the
                 payload (see below). Stored little-endian.
* ``length``   - number of payload bytes, little-endian ``uint16``.
* ``0xAA``     - the ``PETCOL_BYTE`` frame terminator.

The firmware currently uses a weak additive checksum rather than a true CRC.
:func:`petcol_checksum` reproduces it byte-for-byte so this client is
compatible with the *existing* firmware without reflashing. The checksum is
injected into :class:`PetcolClient` as a single function, so swapping in a real
CRC-32 later (once the firmware is updated to match) is a one-line change.

Message types understood by the firmware:

* ``1`` - LCD text. Remaining payload bytes are written to the 20x2 display
          buffer (40 bytes max).
* ``2`` - LED colour. ``payload[1:4]`` are the red, green and blue values used
          for the "online" LED animation.
"""

import struct

PETCOL_BYTE = 0xAA
PACKETSIZE_MAX = 128

MSG_LCD_TEXT = 1
MSG_LED_COLOR = 2


def petcol_checksum(payload):
    """Compute the firmware's additive checksum over ``payload``.

    Mirrors ``petcol::make_CRC`` in ``petprotocol.cpp``: four accumulator
    "lanes" (bytes) collect ``payload[i]`` indexed by ``i % 4``; the two bytes
    of the length are then folded in at the following indices. The lanes are
    packed little-endian into the returned ``uint32``.
    """
    lanes = [0, 0, 0, 0]
    i = 0
    for byte in payload:
        lanes[i % 4] = (lanes[i % 4] + byte) & 0xFF
        i += 1

    length = len(payload)
    len_bytes = (length & 0xFF, (length >> 8) & 0xFF)
    lanes[i % 4] = (lanes[i % 4] + len_bytes[0]) & 0xFF
    i += 1
    lanes[i % 4] = (lanes[i % 4] + len_bytes[1]) & 0xFF

    return lanes[0] | (lanes[1] << 8) | (lanes[2] << 16) | (lanes[3] << 24)


def encode_packet(payload, checksum=petcol_checksum):
    """Frame ``payload`` into a complete petcol packet (``bytes``)."""
    payload = bytes(payload)
    if not payload:
        raise ValueError("petcol payload must be non-empty")
    if len(payload) >= PACKETSIZE_MAX:
        raise ValueError(
            "petcol payload too large: %d (max %d)" % (len(payload), PACKETSIZE_MAX - 1)
        )
    header = struct.pack("<IH", checksum(payload) & 0xFFFFFFFF, len(payload))
    return payload + header + bytes((PETCOL_BYTE,))


class PetcolClient:
    """Frames and writes petcol packets to a byte sink (e.g. a serial port).

    ``transport`` only needs a ``write(data: bytes)`` method, which keeps the
    client testable and lets it run against a fake sink in ``--no-serial`` mode.
    """

    def __init__(self, transport, checksum=petcol_checksum):
        self._transport = transport
        self._checksum = checksum

    def send(self, payload):
        """Encode ``payload`` and write the resulting packet to the transport."""
        packet = encode_packet(payload, self._checksum)
        self._transport.write(packet)
        return packet

    def send_text(self, text):
        """Send a type-1 LCD text message.

        ``text`` may be ``str`` (encoded latin-1, matching the LCD codepage) or
        raw ``bytes``.
        """
        if isinstance(text, str):
            text = text.encode("latin-1", "replace")
        return self.send(bytes((MSG_LCD_TEXT,)) + text)

    def send_color(self, red, green, blue):
        """Send a type-2 LED colour message."""
        rgb = bytes((red & 0xFF, green & 0xFF, blue & 0xFF))
        return self.send(bytes((MSG_LED_COLOR,)) + rgb)
