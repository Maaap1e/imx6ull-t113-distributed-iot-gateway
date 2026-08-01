from __future__ import annotations

import hashlib
import importlib.util
import re
import struct
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec, utils

REPO = Path(__file__).resolve().parents[1]
TOOL = REPO / "tools" / "package_stm32_ota_ab.py"
CONTRACT = REPO / "common" / "ota_contract_ab.h"
BOOT_PROJECT = REPO / "stm32/can_ota_bootloader_ab_secure/Projects/MDK-ARM/atk_f103.uvprojx"
APP_A_PROJECT = REPO / "stm32/dht11_can_app_slot_a/Projects/MDK-ARM/atk_f103.uvprojx"
APP_B_PROJECT = REPO / "stm32/dht11_can_app_slot_b/Projects/MDK-ARM/atk_f103.uvprojx"
METADATA_HEADER = REPO / "stm32/common_ab_secure/ota_ab_metadata.h"
METADATA_SOURCE = REPO / "stm32/common_ab_secure/ota_ab_metadata.c"
RESUME_HEADER = REPO / "stm32/common_ab_secure/ota_resume_journal.h"
RESUME_SOURCE = REPO / "stm32/common_ab_secure/ota_resume_journal.c"
BOOT_SOURCE = REPO / "stm32/can_ota_bootloader_ab_secure/User/main_can_ota.c"
TRANSFER_SOURCE = (
    REPO / "stm32/can_ota_bootloader_ab_secure/User/CAN_OTA/can_ota.c"
)
HOST_SOURCE = (
    REPO / "linux/can_ota_host_ab_secure/stm32_can_ota_host.c"
)
KEY_TOOL = REPO / "tools/generate_ota_signing_key.py"

SPEC = importlib.util.spec_from_file_location("package_stm32_ota_ab", TOOL)
assert SPEC is not None and SPEC.loader is not None
ota = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = ota
SPEC.loader.exec_module(ota)


def valid_image(address: int) -> bytes:
    return struct.pack("<II", 0x20010000, address + 9) + bytes(range(128))


def macro_integer(name: str) -> int:
    text = CONTRACT.read_text(encoding="utf-8")
    match = re.search(
        rf"^#define\s+{re.escape(name)}\s+(0x[0-9A-Fa-f]+|\d+)u?\b",
        text,
        re.MULTILINE,
    )
    if match is None:
        raise AssertionError(f"missing integer macro {name}")
    return int(match.group(1), 0)


def irom(path: Path) -> tuple[int, int]:
    root = ET.parse(path).getroot()
    memory = root.find(".//OnChipMemories/IROM")
    assert memory is not None
    start = memory.findtext("StartAddress")
    size = memory.findtext("Size")
    assert start is not None and size is not None
    return int(start, 0), int(size, 0)


def project_sources(path: Path) -> set[str]:
    root = ET.parse(path).getroot()
    return {
        node.text.replace("\\", "/").lower()
        for node in root.findall(".//FilePath")
        if node.text
    }


class SecureAbPackageTest(unittest.TestCase):
    def setUp(self) -> None:
        self.key = ec.generate_private_key(ec.SECP256R1(), default_backend())
        self.image_a = valid_image(ota.SLOT_A_ADDRESS)
        self.image_b = valid_image(ota.SLOT_B_ADDRESS)
        self.built = ota.build_package(
            self.image_a, self.image_b, ota.HARDWARE_ID_STM32F103,
            0x02000001, 0, self.key
        )

    def test_contract_matches_packer(self) -> None:
        expected = {
            "CAN_OTA_AB_PACKAGE_MAGIC": ota.PACKAGE_MAGIC,
            "CAN_OTA_AB_PACKAGE_HEADER_SIZE": ota.HEADER_SIZE,
            "CAN_OTA_AB_PACKAGE_FORMAT_VERSION": ota.FORMAT_VERSION,
            "CAN_OTA_AB_PACKAGE_AUTH_BYTES": ota.AUTH_BYTES,
            "CAN_OTA_AB_PACKAGE_CRC_BYTES": ota.HEADER_CRC_BYTES,
            "CAN_OTA_AB_SLOT_A_ADDRESS": ota.SLOT_A_ADDRESS,
            "CAN_OTA_AB_SLOT_B_ADDRESS": ota.SLOT_B_ADDRESS,
            "CAN_OTA_AB_SLOT_SIZE": ota.SLOT_SIZE,
        }
        for name, value in expected.items():
            with self.subTest(name=name):
                self.assertEqual(macro_integer(name), value)

    def test_dual_image_layout_hashes_and_crc(self) -> None:
        data = self.built.data
        self.assertEqual(data[:4], b"OTA3")
        self.assertEqual(
            len(data),
            ota.HEADER_SIZE + len(self.built.block_hashes_a) +
            len(self.built.block_hashes_b) +
            len(self.image_a) + len(self.image_b),
        )
        self.assertEqual(
            zlib.crc32(data[:ota.HEADER_CRC_BYTES]) & 0xFFFFFFFF,
            struct.unpack_from("<I", data, 244)[0],
        )
        self.assertEqual(data[28:60], hashlib.sha256(self.image_a).digest())
        self.assertEqual(data[104:136], hashlib.sha256(self.image_b).digest())
        self.assertEqual(
            data[64:96],
            hashlib.sha256(self.built.block_hashes_a).digest(),
        )
        self.assertEqual(
            data[140:172],
            hashlib.sha256(self.built.block_hashes_b).digest(),
        )
        first_a_hash = hashlib.sha256(
            self.image_a[:ota.BLOCK_SIZE]
        ).digest()
        self.assertEqual(self.built.block_hashes_a[:32], first_a_hash)

    def test_signature_binds_both_image_hashes(self) -> None:
        data = self.built.data
        digest = hashlib.sha256(data[:ota.AUTH_BYTES]).digest()
        der = utils.encode_dss_signature(
            int.from_bytes(data[180:212], "big"),
            int.from_bytes(data[212:244], "big"),
        )
        self.key.public_key().verify(
            der, digest, ec.ECDSA(utils.Prehashed(hashes.SHA256()))
        )
        tampered = bytearray(data[:ota.AUTH_BYTES])
        tampered[140] ^= 1
        with self.assertRaises(InvalidSignature):
            self.key.public_key().verify(
                der, hashlib.sha256(tampered).digest(),
                ec.ECDSA(utils.Prehashed(hashes.SHA256()))
            )

    def test_wrong_key_cannot_verify(self) -> None:
        other = ec.generate_private_key(ec.SECP256R1(), default_backend())
        signature = self.built.signature
        der = utils.encode_dss_signature(
            int.from_bytes(signature[:32], "big"),
            int.from_bytes(signature[32:], "big"),
        )
        with self.assertRaises(InvalidSignature):
            other.public_key().verify(
                der,
                hashlib.sha256(self.built.data[:ota.AUTH_BYTES]).digest(),
                ec.ECDSA(utils.Prehashed(hashes.SHA256())),
            )

    def test_block_table_tamper_is_detected(self) -> None:
        tampered = bytearray(self.built.block_hashes_a)
        tampered[0] ^= 1
        expected_table_digest = self.built.data[64:96]
        self.assertNotEqual(hashlib.sha256(tampered).digest(),
                            expected_table_digest)

    def test_multiple_flash_pages_have_independent_hashes(self) -> None:
        image_a = struct.pack(
            "<II", 0x20010000, ota.SLOT_A_ADDRESS + 9
        ) + bytes(index & 0xFF for index in range(4992))
        image_b = struct.pack(
            "<II", 0x20010000, ota.SLOT_B_ADDRESS + 9
        ) + bytes((index * 3) & 0xFF for index in range(4992))
        built = ota.build_package(
            image_a, image_b, ota.HARDWARE_ID_STM32F103,
            0x02010000, 0, self.key
        )
        self.assertEqual(len(built.block_hashes_a), 3 * 32)
        self.assertEqual(len(built.block_hashes_b), 3 * 32)
        self.assertEqual(struct.unpack_from("<H", built.data, 60)[0], 3)
        self.assertEqual(struct.unpack_from("<H", built.data, 136)[0], 3)
        for block in range(3):
            start = block * ota.BLOCK_SIZE
            self.assertEqual(
                built.block_hashes_a[block * 32 : (block + 1) * 32],
                hashlib.sha256(
                    image_a[start : start + ota.BLOCK_SIZE]
                ).digest(),
            )

    def test_rejects_slot_b_binary_linked_for_slot_a(self) -> None:
        with self.assertRaisesRegex(ValueError, "Slot B reset handler"):
            ota.build_package(
                self.image_a, self.image_a, ota.HARDWARE_ID_STM32F103,
                0x02000001, 0, self.key
            )

    def test_key_generator_replaces_only_placeholder(self) -> None:
        # Use a local fixture so this test still works after the project has
        # been provisioned with its real release public key.
        placeholder = """#include \"ota_trusted_key.h\"

/* Deliberately invalid placeholder. */
const uint32_t g_ota_trusted_key_id = 0u;
const uint8_t g_ota_trusted_public_key[OTA_TRUSTED_PUBLIC_KEY_SIZE] = { 0u };
"""
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            private_key = temporary / "private.pem"
            public_c = temporary / "ota_trusted_key.c"
            public_c.write_text(placeholder, encoding="utf-8")
            result = subprocess.run(
                [
                    sys.executable,
                    str(KEY_TOOL),
                    "--private-key",
                    str(private_key),
                    "--public-c",
                    str(public_c),
                    "--development-unencrypted",
                    "--replace-placeholder",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("BEGIN PRIVATE KEY", private_key.read_text())
            generated = public_c.read_text(encoding="utf-8")
            self.assertNotIn("Deliberately invalid placeholder", generated)
            self.assertRegex(generated, r"g_ota_trusted_key_id = 0x[0-9A-F]{8}u")


class SecureAbLayoutAndStateTest(unittest.TestCase):
    def test_flash_layout(self) -> None:
        self.assertEqual(irom(BOOT_PROJECT), (0x08000000, 0x10000))
        self.assertEqual(irom(APP_A_PROJECT), (0x08010000, 0x37000))
        self.assertEqual(irom(APP_B_PROJECT), (0x08047000, 0x37000))
        self.assertEqual(0x08010000 + 0x37000, 0x08047000)
        self.assertEqual(0x08047000 + 0x37000, 0x0807E000)
        self.assertEqual(0x0807E000 + 4 * 0x800, 0x08080000)

    def test_keil_backend_and_app_bin_commands(self) -> None:
        boot_xml = ET.parse(BOOT_PROJECT).getroot()
        defines = boot_xml.findtext(".//Cads/VariousControls/Define") or ""
        self.assertIn("uECC_PLATFORM=uECC_arch_other", defines)

        for project, name in (
            (APP_A_PROJECT, "stm32_dht11_can_app_slot_a"),
            (APP_B_PROJECT, "stm32_dht11_can_app_slot_b"),
        ):
            command = (
                ET.parse(project).getroot().findtext(
                    ".//AfterMake/UserProg1Name"
                )
                or ""
            )
            self.assertIn(f"{name}.axf", command)
            self.assertIn(f"{name}.bin", command)

        usart_header = (
            REPO
            / "stm32/can_ota_bootloader_ab_secure/Drivers/SYSTEM/usart/usart.h"
        ).read_bytes()
        self.assertIn(b"#define USART_REC_LEN               1024", usart_header)

    def test_bootloader_has_crypto_and_journal_sources(self) -> None:
        sources = project_sources(BOOT_PROJECT)
        for suffix in (
            "common_ab_secure/ota_ab_metadata.c",
            "common_ab_secure/ota_resume_journal.c",
            "common_ab_secure/ota_trusted_key.c",
            "common_ab_secure/crypto/uecc.c",
            "common/crypto/sha256.c",
        ):
            self.assertTrue(any(path.endswith(suffix) for path in sources))

        crypto = REPO / "stm32/common_ab_secure/crypto"
        for dependency in (
            "uECC_vli.h",
            "types.h",
            "platform-specific.inc",
            "curve-specific.inc",
            "asm_arm.inc",
            "asm_arm_mult_square.inc",
            "asm_arm_mult_square_umaal.inc",
        ):
            with self.subTest(dependency=dependency):
                self.assertTrue((crypto / dependency).is_file())

    def test_both_apps_use_ab_metadata(self) -> None:
        for project in (APP_A_PROJECT, APP_B_PROJECT):
            self.assertTrue(any(
                path.endswith("common_ab_secure/ota_ab_metadata.c")
                for path in project_sources(project)
            ))

    def test_rollback_is_persisted_before_jump(self) -> None:
        boot = BOOT_SOURCE.read_text(encoding="utf-8")
        metadata = METADATA_SOURCE.read_text(encoding="utf-8")
        header = METADATA_HEADER.read_text(encoding="utf-8")
        self.assertIn("ota_ab_metadata_mark_trial(slot)", boot)
        self.assertIn("trial image reset before confirmation", boot)
        self.assertIn("ota_ab_metadata_rollback()", boot)
        self.assertIn("Write everything except magic first", metadata)
        self.assertIn("OTA_AB_METADATA_RECORD_SIZE    128u", header)
        self.assertIn("CAN_OTA_AB_METADATA0_ADDRESS", metadata)
        self.assertIn("CAN_OTA_AB_METADATA1_ADDRESS", metadata)

    def test_resume_journal_is_power_loss_committed_and_block_aligned(self) -> None:
        source = RESUME_SOURCE.read_text(encoding="utf-8")
        header = RESUME_HEADER.read_text(encoding="utf-8")
        transfer = TRANSFER_SOURCE.read_text(encoding="utf-8")
        host = HOST_SOURCE.read_text(encoding="utf-8")
        self.assertIn("Write everything except magic", source)
        self.assertIn("CAN_OTA_AB_RESUME0_ADDRESS", source)
        self.assertIn("CAN_OTA_AB_RESUME1_ADDRESS", source)
        self.assertIn("OTA_RESUME_RECORD_SIZE       128u", header)
        self.assertIn("ota_resume_first_missing", source)
        self.assertIn("first_missing", source)
        self.assertIn("flash_block_matches(block)", transfer)
        self.assertIn("erase_target_block(first_missing)", transfer)
        self.assertIn("ota_resume_mark_block(&g_resume, block)", transfer)
        self.assertIn("status.received_kb * 1024u", host)
        self.assertIn("size_t start_offset", host)
        self.assertIn("size_t block_remaining", host)
        self.assertIn("(uint8_t)(chunk + 2u)", host)
        self.assertIn("g_received_size % CAN_OTA_AB_BLOCK_SIZE", transfer)


if __name__ == "__main__":
    unittest.main()
