import importlib.util
import re
import struct
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
TOOL_PATH = REPO / "tools" / "package_stm32_ota.py"
CONTRACT_PATH = REPO / "common" / "ota_contract.h"
SPEC = importlib.util.spec_from_file_location("package_stm32_ota", TOOL_PATH)
assert SPEC is not None and SPEC.loader is not None
ota = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ota)


def valid_image() -> bytes:
    vector = struct.pack("<II", 0x20010000, ota.APP_ADDRESS + 0x09)
    return vector + bytes(range(64))


def contract_integer(name: str) -> int:
    text = CONTRACT_PATH.read_text(encoding="utf-8")
    match = re.search(
        rf"^#define\s+{re.escape(name)}\s+(0x[0-9A-Fa-f]+|\d+)u?\b",
        text,
        re.MULTILINE,
    )
    if match is None:
        raise AssertionError(f"integer macro {name} missing from OTA contract")
    return int(match.group(1), 0)


class OtaPackageToolTest(unittest.TestCase):
    def test_python_constants_match_shared_contract(self) -> None:
        expected = {
            "CAN_OTA_PACKAGE_MAGIC": ota.PACKAGE_MAGIC,
            "CAN_OTA_PACKAGE_HEADER_SIZE": ota.HEADER_SIZE,
            "CAN_OTA_PACKAGE_FORMAT_VERSION": ota.FORMAT_VERSION,
            "CAN_OTA_HARDWARE_ID_STM32F103": ota.HARDWARE_ID_STM32F103,
            "CAN_OTA_MANIFEST_ALLOW_DOWNGRADE": ota.FLAG_ALLOW_DOWNGRADE,
            "CAN_OTA_APP_ADDRESS": ota.APP_ADDRESS,
            "CAN_OTA_APP_MAX_SIZE": ota.APP_MAX_SIZE,
            "CAN_OTA_SRAM_START": ota.SRAM_START,
            "CAN_OTA_SRAM_END": ota.SRAM_END,
        }
        for name, value in expected.items():
            with self.subTest(name=name):
                self.assertEqual(contract_integer(name), value)

    def test_version_encoding(self) -> None:
        encoded, normalized = ota.parse_version("1.2.3")
        self.assertEqual(encoded, 0x01020300)
        self.assertEqual(normalized, "1.2.3.0")

    def test_package_layout_and_crc(self) -> None:
        image = valid_image()
        ota.validate_image(image)
        package, image_crc, header_crc = ota.build_package(
            image, ota.HARDWARE_ID_STM32F103, 0x01020000, 0
        )
        self.assertEqual(len(package), ota.HEADER_SIZE + len(image))
        self.assertEqual(package[:4], b"COTA")
        self.assertEqual(
            struct.unpack_from("<I", package, 20)[0],
            image_crc,
        )
        self.assertEqual(
            struct.unpack_from("<I", package, 28)[0],
            header_crc,
        )

    def test_rejects_vector_outside_app(self) -> None:
        image = struct.pack("<II", 0x20010000, 0x08000001) + bytes(16)
        with self.assertRaisesRegex(ValueError, "outside the image"):
            ota.validate_image(image)

    def test_atomic_write_refuses_overwrite_without_force(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "firmware.ota"
            ota.write_atomic(output, b"one", force=False)
            with self.assertRaises(FileExistsError):
                ota.write_atomic(output, b"two", force=False)
            ota.write_atomic(output, b"two", force=True)
            self.assertEqual(output.read_bytes(), b"two")


if __name__ == "__main__":
    unittest.main()
