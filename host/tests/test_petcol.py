# coding: utf-8
"""Tests for the petcol client.

The key test compiles ``crc_reference.c`` (a verbatim copy of the firmware's
``make_CRC``) and asserts the Python checksum matches it for many payloads, so
the two implementations cannot silently drift apart.
"""

import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from petcol import (  # noqa: E402
    PETCOL_BYTE,
    PetcolClient,
    PetcolDecoder,
    encode_packet,
    petcol_checksum,
)


def py_decode(stream, delimiter=PETCOL_BYTE):
    """Run the pure-Python decoder, returning (packets, extra_bytes)."""
    extra = bytearray()
    decoder = PetcolDecoder(delimiter=delimiter, on_extra=extra.append)
    packets = decoder.feed(bytes(stream))
    return packets, bytes(extra)

HERE = os.path.dirname(os.path.abspath(__file__))
FIRMWARE = os.path.join(os.path.dirname(os.path.dirname(HERE)), "limesplay_firmware")


class _Recorder:
    def __init__(self):
        self.chunks = []

    def write(self, data):
        self.chunks.append(bytes(data))


class EncodeTests(unittest.TestCase):
    def test_trailer_layout(self):
        payload = b"\x01Hello"
        packet = encode_packet(payload)
        self.assertEqual(packet[: len(payload)], payload)
        crc, length = struct.unpack("<IH", packet[len(payload):len(payload) + 6])
        self.assertEqual(length, len(payload))
        self.assertEqual(crc, petcol_checksum(payload))
        self.assertEqual(packet[-1], PETCOL_BYTE)

    def test_custom_delimiter(self):
        payload = b"\x01Hello"
        packet = encode_packet(payload, delimiter=0x55)
        self.assertEqual(packet[-1], 0x55)
        # default is unchanged
        self.assertEqual(encode_packet(payload)[-1], PETCOL_BYTE)

    def test_client_uses_delimiter(self):
        rec = _Recorder()
        PetcolClient(rec, delimiter=0x55).send_text("hi")
        self.assertEqual(rec.chunks[0][-1], 0x55)

    def test_known_crc32_vector(self):
        # Canonical CRC-32 check value for the ASCII string "123456789".
        self.assertEqual(petcol_checksum(b"123456789"), 0xCBF43926)

    def test_rejects_empty_and_oversize(self):
        with self.assertRaises(ValueError):
            encode_packet(b"")
        with self.assertRaises(ValueError):
            encode_packet(b"\x00" * 128)

    def test_client_helpers(self):
        rec = _Recorder()
        client = PetcolClient(rec)
        client.send_text("hi")
        client.send_color(255, 0, 128)
        text_pkt, color_pkt = rec.chunks
        self.assertEqual(text_pkt[0], 1)
        self.assertEqual(text_pkt[1:3], b"hi")
        self.assertEqual(color_pkt[:4], bytes((2, 255, 0, 128)))


class PythonDecoderTests(unittest.TestCase):
    """The pure-Python decoder, independent of any C toolchain."""

    def test_roundtrip_and_extra_in_order(self):
        extra1 = b"hello "
        pkt1 = encode_packet(b"\x01Hi")
        extra2 = b"\x00\xff debug\n"
        pkt2 = encode_packet(bytes((2, 255, 0, 128)))
        # payload that itself contains the delimiter byte (false-positive guard)
        pkt3 = encode_packet(b"\x01" + bytes((PETCOL_BYTE, PETCOL_BYTE, 0x10)))

        packets, extra = py_decode(extra1 + pkt1 + extra2 + pkt2 + pkt3)
        self.assertEqual(packets, [
            b"\x01Hi",
            bytes((2, 255, 0, 128)),
            b"\x01" + bytes((PETCOL_BYTE, PETCOL_BYTE, 0x10)),
        ])
        self.assertEqual(extra, extra1 + extra2)

    def test_custom_delimiter(self):
        packets, extra = py_decode(
            b"x" + encode_packet(b"\x01ok", delimiter=0x00), delimiter=0x00)
        self.assertEqual(packets, [b"\x01ok"])
        self.assertEqual(extra, b"x")

    def test_corrupt_crc_is_not_decoded(self):
        pkt = bytearray(encode_packet(b"\x01payload"))
        pkt[0] ^= 0xFF
        packets, _ = py_decode(bytes(pkt))
        self.assertEqual(packets, [])

    def test_max_size_packet_with_leading_extra(self):
        extra = b"extra" * 10
        payload = bytes(range(127))
        packets, got_extra = py_decode(extra + encode_packet(payload))
        self.assertEqual(packets, [payload])
        self.assertEqual(got_extra, extra)

    def test_partial_frame_is_buffered_not_emitted(self):
        # A packet split across two feeds must still decode, and nothing should
        # leak out as extra before the delimiter arrives.
        pkt = encode_packet(b"\x01chunked")
        extra = bytearray()
        decoder = PetcolDecoder(on_extra=extra.append)
        self.assertEqual(decoder.feed(pkt[:4]), [])
        self.assertEqual(extra, b"")
        self.assertEqual(decoder.feed(pkt[4:]), [b"\x01chunked"])
        self.assertEqual(bytes(extra), b"")


class ChecksumMatchesFirmwareTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = shutil.which("cc") or shutil.which("gcc")
        if not cc:
            raise unittest.SkipTest("no C compiler available")
        cls.tmpdir = tempfile.mkdtemp()
        cls.binary = os.path.join(cls.tmpdir, "crc_reference")
        subprocess.check_call([cc, "-O2", "-o", cls.binary,
                               os.path.join(HERE, "crc_reference.c")])

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmpdir, ignore_errors=True)

    def _firmware_crc(self, payload):
        out = subprocess.check_output([self.binary, payload.hex()])
        return int(out.strip())

    def test_matches_for_many_payloads(self):
        payloads = [
            b"\x01",
            b"\x01Hello world",
            b"\x02\xff\x00\x80",
            bytes(range(127)),
            b"\xaa" * 50,
            b"limesplay rocks \xc3\xa5\xc3\xa4\xc3\xb6",
        ]
        for payload in payloads:
            self.assertEqual(
                petcol_checksum(payload),
                self._firmware_crc(payload),
                "mismatch for payload %r" % payload,
            )


class FirmwareDecoderTests(unittest.TestCase):
    """Compile the firmware petcol decoder and feed it frames from petcol.py."""

    @classmethod
    def setUpClass(cls):
        cxx = shutil.which("g++") or shutil.which("c++")
        if not cxx:
            raise unittest.SkipTest("no C++ compiler available")
        cls.tmpdir = tempfile.mkdtemp()
        cls.binary = os.path.join(cls.tmpdir, "recv_harness")
        subprocess.check_call([
            cxx, "-std=c++11", "-O2", "-I", FIRMWARE,
            "-o", cls.binary,
            os.path.join(HERE, "recv_harness.cpp"),
            os.path.join(FIRMWARE, "petprotocol.cpp"),
        ])

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmpdir, ignore_errors=True)

    def _decode(self, stream):
        out = subprocess.check_output([self.binary], input=bytes(stream))
        packets, extra = [], b""
        for line in out.decode().splitlines():
            parts = line.split()
            if parts and parts[0] == "PKT":
                packets.append(bytes(int(h, 16) for h in parts[1:]))
            elif parts and parts[0] == "EXTRA":
                extra = bytes(int(h, 16) for h in parts[1:])
        return packets, extra

    def test_decodes_frames_and_passes_extra(self):
        extra1 = b"hello "
        pkt1 = encode_packet(b"\x01Hi")
        extra2 = b"\x00\xff debug\n"
        pkt2 = encode_packet(bytes((2, 255, 0, 128)))
        # payload that itself contains the delimiter byte (false-positive test)
        pkt3 = encode_packet(b"\x01" + bytes((PETCOL_BYTE, PETCOL_BYTE, 0x10)))

        stream = extra1 + pkt1 + extra2 + pkt2 + pkt3
        packets, extra = self._decode(stream)

        self.assertEqual(packets, [
            b"\x01Hi",
            bytes((2, 255, 0, 128)),
            b"\x01" + bytes((PETCOL_BYTE, PETCOL_BYTE, 0x10)),
        ])
        # Extra bytes preceding a matched packet are handed back in arrival order.
        self.assertEqual(extra, extra1 + extra2)

    def test_corrupt_crc_is_not_decoded(self):
        pkt = bytearray(encode_packet(b"\x01payload"))
        pkt[0] ^= 0xFF  # corrupt the first payload byte; CRC no longer matches
        packets, _ = self._decode(bytes(pkt))
        self.assertEqual(packets, [])

    def test_max_size_packet_with_leading_extra(self):
        # A near-max payload spans more than a max-size buffer window; the leading
        # extra must spill out while the whole frame is still recognised.
        extra = b"extra"* 10                # 50 bytes, no delimiter
        payload = bytes(range(127))         # max payload (length < PACKETSIZE_MAX)
        packets, got_extra = self._decode(extra + encode_packet(payload))
        self.assertEqual(packets, [payload])
        self.assertEqual(got_extra, extra)

    def test_large_extra_stream_all_delivered_in_order(self):
        # Lots of non-packet data ahead of a packet: every byte must reach the
        # extra callback in order (some via the age-out spill, some via the match
        # flush), and nothing may be dropped.
        extra = bytes((i * 3 + 1) & 0x7F for i in range(300))  # 300 bytes, no 0xAA
        payload = b"\x01done"
        packets, got_extra = self._decode(extra + encode_packet(payload))
        self.assertEqual(packets, [payload])
        self.assertEqual(got_extra, extra)

    def test_python_decoder_matches_firmware(self):
        # The pure-Python decoder must reproduce the firmware decoder byte for
        # byte (same packets, same extra) across a range of streams.
        streams = [
            b"hello " + encode_packet(b"\x01Hi") + b"\x00\xff bye\n"
            + encode_packet(bytes((2, 1, 2, 3))),
            encode_packet(b"\x01" + bytes((PETCOL_BYTE, PETCOL_BYTE, 9))),
            bytearray(encode_packet(b"\x01nope")),  # corrupted below
            b"extra" * 10 + encode_packet(bytes(range(127))),
            bytes((i * 3 + 1) & 0x7F for i in range(300)) + encode_packet(b"\x01x"),
            bytes([PETCOL_BYTE]) * 8 + encode_packet(b"\x01\x01\x01"),
        ]
        streams[2][0] ^= 0xFF
        for stream in streams:
            fw_packets, fw_extra = self._decode(stream)
            py_packets, py_extra = py_decode(stream)
            self.assertEqual(py_packets, fw_packets, "packet mismatch on %r" % bytes(stream))
            self.assertEqual(py_extra, fw_extra, "extra mismatch on %r" % bytes(stream))


if __name__ == "__main__":
    unittest.main()
