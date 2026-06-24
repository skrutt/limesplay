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
    encode_packet,
    petcol_checksum,
)

HERE = os.path.dirname(os.path.abspath(__file__))


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


if __name__ == "__main__":
    unittest.main()
