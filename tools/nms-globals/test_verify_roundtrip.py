import unittest

from verify_roundtrip import FORMAT_3300, METADATA_OFFSETS, preserve_metadata


SHIPPED = FORMAT_3300 + b"\0\0" + b"synthetic-12" + bytes.fromhex(
    "f1f2f3f4f5000000"
) + b"synthetic payload"
STOCK = SHIPPED[:10] + b"\x04" + SHIPPED[11:24] + bytes.fromhex(
    "070a000100"
) + SHIPPED[29:]


class RoundtripTests(unittest.TestCase):
    def test_restores_only_six_metadata_bytes(self):
        output = preserve_metadata(SHIPPED, STOCK)
        self.assertEqual(output, SHIPPED)
        self.assertEqual({i for i, (old, new) in enumerate(zip(STOCK, output))
                          if old != new}, METADATA_OFFSETS)

    def test_refuses_schema_header_and_payload_differences(self):
        for offset in (0, 0x08, 0x0B, 0x0C, 0x10, 0x1D, 0x20):
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                altered = STOCK[:offset] + bytes([STOCK[offset] ^ 1]) + STOCK[offset + 1:]
                preserve_metadata(SHIPPED, altered)

    def test_refuses_truncated_stock(self):
        with self.assertRaisesRegex(ValueError, "length"):
            preserve_metadata(SHIPPED, STOCK[:-1])


if __name__ == "__main__":
    unittest.main()
