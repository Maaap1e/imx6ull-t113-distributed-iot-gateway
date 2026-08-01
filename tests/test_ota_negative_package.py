import struct
import sys
import unittest
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import make_ota_negative_test as negative


class NegativePackageTests(unittest.TestCase):
    def test_signature_mutation_preserves_header_crc(self) -> None:
        package = bytearray(b"\x00" * 512)
        package[negative.SIGNATURE_OFFSET :
                negative.SIGNATURE_OFFSET + negative.SIGNATURE_SIZE] = bytes(
                    range(negative.SIGNATURE_SIZE)
                )
        crc = zlib.crc32(package[:negative.HEADER_CRC_OFFSET]) & 0xFFFFFFFF
        struct.pack_into("<I", package, negative.HEADER_CRC_OFFSET, crc)

        result = negative.corrupt_signature(bytes(package))

        self.assertEqual(result[:negative.SIGNATURE_OFFSET],
                         package[:negative.SIGNATURE_OFFSET])
        self.assertNotEqual(
            result[negative.SIGNATURE_OFFSET:
                   negative.SIGNATURE_OFFSET + negative.SIGNATURE_SIZE],
            package[negative.SIGNATURE_OFFSET:
                    negative.SIGNATURE_OFFSET + negative.SIGNATURE_SIZE],
        )
        self.assertEqual(result[negative.HEADER_SIZE:],
                         package[negative.HEADER_SIZE:])
        stored = struct.unpack_from(
            "<I", result, negative.HEADER_CRC_OFFSET
        )[0]
        self.assertEqual(
            stored,
            zlib.crc32(result[:negative.HEADER_CRC_OFFSET]) & 0xFFFFFFFF,
        )

    def test_rejects_already_corrupt_header(self) -> None:
        with self.assertRaisesRegex(ValueError, "already invalid"):
            negative.corrupt_signature(b"\x00" * 512)

    def test_key_id_mutation_preserves_header_crc(self) -> None:
        package = bytearray(b"\x00" * 512)
        struct.pack_into("<I", package, negative.KEY_ID_OFFSET, 0x5A876252)
        crc = zlib.crc32(package[:negative.HEADER_CRC_OFFSET]) & 0xFFFFFFFF
        struct.pack_into("<I", package, negative.HEADER_CRC_OFFSET, crc)

        result = negative.corrupt_key_id(bytes(package))

        self.assertEqual(
            struct.unpack_from("<I", result, negative.KEY_ID_OFFSET)[0],
            0x5A876253,
        )
        stored = struct.unpack_from(
            "<I", result, negative.HEADER_CRC_OFFSET
        )[0]
        self.assertEqual(
            stored,
            zlib.crc32(result[:negative.HEADER_CRC_OFFSET]) & 0xFFFFFFFF,
        )

    def test_hardware_id_mutation_preserves_header_crc(self) -> None:
        package = bytearray(b"\x00" * 512)
        struct.pack_into("<H", package, negative.HARDWARE_ID_OFFSET, 0xF103)
        crc = zlib.crc32(package[:negative.HEADER_CRC_OFFSET]) & 0xFFFFFFFF
        struct.pack_into("<I", package, negative.HEADER_CRC_OFFSET, crc)

        result = negative.corrupt_hardware_id(bytes(package))

        self.assertEqual(
            struct.unpack_from("<H", result,
                               negative.HARDWARE_ID_OFFSET)[0],
            0xF102,
        )
        stored = struct.unpack_from(
            "<I", result, negative.HEADER_CRC_OFFSET
        )[0]
        self.assertEqual(
            stored,
            zlib.crc32(result[:negative.HEADER_CRC_OFFSET]) & 0xFFFFFFFF,
        )


if __name__ == "__main__":
    unittest.main()
