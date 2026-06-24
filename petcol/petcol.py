# coding: utf-8
"""Python client for the *petcol* serial protocol used by the limesplay firmware.

Wire format (as implemented by ``petprotocol.cpp`` in the firmware)::

    [ payload bytes ][ checksum u32 LE ][ length u16 LE ][ 0xAA ]

* ``payload``  - the message bytes. The first byte is the message *type*.
* ``checksum`` - CRC-32 (IEEE 802.3 / zlib) of the payload bytes, stored
                 little-endian.
* ``length``   - number of payload bytes, little-endian ``uint16``.
* ``0xAA``     - the ``PETCOL_BYTE`` frame terminator.

:func:`petcol_checksum` is a standard CRC-32, matching ``petcol::make_CRC`` in
the firmware byte-for-byte. It is injected into :class:`PetcolClient` as a
single function, so an alternative checksum can be swapped in without touching
the rest of the client.

Message types understood by the firmware:

* ``1`` - LCD text. Remaining payload bytes are written to the 20x2 display
          buffer (40 bytes max).
* ``2`` - LED colour. ``payload[1:4]`` are the red, green and blue values used
          for the "online" LED animation.
"""

import struct
import zlib

PETCOL_BYTE = 0xAA
PACKETSIZE_MAX = 128

# Trailer carried on the wire after the payload: crc (4) + length (2). The
# delimiter is not buffered by the receiver, so it is not counted here.
PETCOL_TRAILER = 6
# Largest frame that can be pending in the receive buffer before its delimiter
# arrives: the biggest legal payload plus the trailer. Mirrors the firmware.
PETCOL_MAX_PENDING = (PACKETSIZE_MAX - 1) + PETCOL_TRAILER

MSG_LCD_TEXT = 1
MSG_LED_COLOR = 2


def petcol_checksum(payload):
    """CRC-32 (IEEE 802.3 / zlib) of ``payload``.

    Identical to ``petcol::make_CRC`` in ``petprotocol.cpp`` (reflected,
    polynomial ``0xEDB88320``).
    """
    return zlib.crc32(bytes(payload)) & 0xFFFFFFFF


def encode_packet(payload, checksum=petcol_checksum, delimiter=PETCOL_BYTE):
    """Frame ``payload`` into a complete petcol packet (``bytes``).

    ``delimiter`` is the frame terminator byte; it must match the instance the
    receiving firmware was constructed with (defaults to ``PETCOL_BYTE``).
    """
    payload = bytes(payload)
    if not payload:
        raise ValueError("petcol payload must be non-empty")
    if len(payload) >= PACKETSIZE_MAX:
        raise ValueError(
            "petcol payload too large: %d (max %d)" % (len(payload), PACKETSIZE_MAX - 1)
        )
    header = struct.pack("<IH", checksum(payload) & 0xFFFFFFFF, len(payload))
    return payload + header + bytes((delimiter & 0xFF,))


class PetcolClient:
    """Frames and writes petcol packets to a byte sink (e.g. a serial port).

    ``transport`` only needs a ``write(data: bytes)`` method, which keeps the
    client testable and lets it run against a fake sink in ``--no-serial`` mode.

    A non-default ``delimiter`` must match the delimiter the receiving firmware
    instance was constructed with.
    """

    def __init__(self, transport, checksum=petcol_checksum, delimiter=PETCOL_BYTE):
        self._transport = transport
        self._checksum = checksum
        self._delimiter = delimiter

    def send(self, payload):
        """Encode ``payload`` and write the resulting packet to the transport."""
        packet = encode_packet(payload, self._checksum, self._delimiter)
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


class PetcolDecoder:
    """Decodes a petcol byte stream - the host-side mirror of the firmware's
    ``recv_byte_input``.

    Feed incoming bytes with :meth:`feed`; it returns the list of complete,
    CRC-validated packets found in that chunk. Every byte that is *not* part of a
    packet is handed to ``on_extra`` (one byte at a time, in arrival order), so a
    petcol channel can be separated from other traffic sharing the same stream.

    The algorithm matches the firmware exactly: bytes accumulate in a buffer, and
    when a byte equals the ``delimiter`` the candidate trailing frame is inspected
    in place; the frame is only consumed once its CRC checks out. A delimiter that
    does not complete a valid frame is treated as ordinary data, and any byte too
    old to still belong to a pending frame is released as extra data, so nothing
    is ever dropped.
    """

    def __init__(self, checksum=petcol_checksum, delimiter=PETCOL_BYTE,
                 on_packet=None, on_extra=None):
        self._checksum = checksum
        self._delimiter = delimiter & 0xFF
        self._on_packet = on_packet
        self._on_extra = on_extra
        self._buf = bytearray()

    def feed(self, data):
        """Feed one byte (``int``) or many (``bytes``); return decoded packets."""
        if isinstance(data, int):
            data = (data,)
        packets = []
        for byte in data:
            packet = self._feed_byte(byte & 0xFF)
            if packet is not None:
                packets.append(packet)
        return packets

    def _emit_extra(self, data):
        if self._on_extra is not None:
            for byte in data:
                self._on_extra(byte)

    def _feed_byte(self, byte):
        buf = self._buf
        if byte == self._delimiter and len(buf) >= PETCOL_TRAILER:
            count = len(buf)
            length = buf[count - 2] | (buf[count - 1] << 8)
            if 0 < length < PACKETSIZE_MAX and count >= length + PETCOL_TRAILER:
                crc_pos = count - PETCOL_TRAILER
                payload_pos = crc_pos - length
                want_crc = (buf[crc_pos]
                            | (buf[crc_pos + 1] << 8)
                            | (buf[crc_pos + 2] << 16)
                            | (buf[crc_pos + 3] << 24))
                payload = bytes(buf[payload_pos:payload_pos + length])
                if self._checksum(payload) == want_crc:
                    # Bytes before the payload are non-packet data; the frame
                    # itself (payload + trailer) is consumed.
                    self._emit_extra(buf[:payload_pos])
                    self._buf = bytearray()
                    if self._on_packet is not None:
                        self._on_packet(payload)
                    return payload

        # Not a frame boundary (or not a valid frame): ordinary data.
        buf.append(byte)

        # Release any byte too old to still be part of a pending frame.
        while len(buf) > PETCOL_MAX_PENDING:
            self._emit_extra(buf[:1])
            del buf[0]

        return None
