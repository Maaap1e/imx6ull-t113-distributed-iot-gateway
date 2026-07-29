from __future__ import annotations

import unittest
import xml.etree.ElementTree as ET
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
BOOT_PROJECT = (
    REPO
    / "stm32"
    / "can_ota_bootloader"
    / "Projects"
    / "MDK-ARM"
    / "atk_f103.uvprojx"
)
APP_PROJECT = (
    REPO
    / "stm32"
    / "dht11_can_app"
    / "Projects"
    / "MDK-ARM"
    / "atk_f103.uvprojx"
)
METADATA_SOURCE = REPO / "stm32" / "common" / "ota_metadata.c"
METADATA_HEADER = REPO / "stm32" / "common" / "ota_metadata.h"


def project_irom(path: Path) -> tuple[int, int]:
    root = ET.parse(path).getroot()
    irom = root.find(".//OnChipMemories/IROM")
    if irom is None:
        raise AssertionError(f"IROM configuration missing from {path}")
    start = irom.findtext("StartAddress")
    size = irom.findtext("Size")
    if start is None or size is None:
        raise AssertionError(f"incomplete IROM configuration in {path}")
    return int(start, 0), int(size, 0)


def project_sources(path: Path) -> set[str]:
    root = ET.parse(path).getroot()
    return {
        source.replace("\\", "/").lower()
        for source in (
            node.text for node in root.findall(".//FilePath") if node.text
        )
    }


class OtaFlashLayoutTest(unittest.TestCase):
    def test_bootloader_does_not_overlap_app(self) -> None:
        self.assertEqual(project_irom(BOOT_PROJECT), (0x08000000, 0x00010000))

    def test_app_stops_before_metadata_page(self) -> None:
        app_start, app_size = project_irom(APP_PROJECT)
        self.assertEqual((app_start, app_size), (0x08010000, 0x0006F800))
        self.assertEqual(app_start + app_size, 0x0807F800)

    def test_bootloader_contains_metadata_and_flash_support(self) -> None:
        sources = project_sources(BOOT_PROJECT)
        self.assertTrue(
            any(source.endswith("common/ota_metadata.c") for source in sources)
        )
        self.assertTrue(
            any(source.endswith("stm32f1xx_hal_flash.c") for source in sources)
        )
        self.assertTrue(
            any(source.endswith("stm32f1xx_hal_flash_ex.c") for source in sources)
        )

    def test_app_can_confirm_metadata(self) -> None:
        sources = project_sources(APP_PROJECT)
        self.assertTrue(
            any(source.endswith("common/ota_metadata.c") for source in sources)
        )
        self.assertTrue(
            any(source.endswith("stm32f1xx_hal_flash.c") for source in sources)
        )
        self.assertTrue(
            any(source.endswith("stm32f1xx_hal_flash_ex.c") for source in sources)
        )

    def test_state_transitions_use_distinct_erased_halfwords(self) -> None:
        source = METADATA_SOURCE.read_text(encoding="utf-8")
        header = METADATA_HEADER.read_text(encoding="utf-8")

        self.assertIn("OTA_METADATA_TRIAL_MARKER_ADDR", source)
        self.assertIn("OTA_METADATA_CONFIRMED_MARKER_ADDR", source)
        self.assertIn("OTA_METADATA_MARKER_ERASED", source)
        self.assertNotIn(
            "offsetof(ota_boot_metadata_t, state)",
            source,
        )
        self.assertIn(
            "STM32F1 rejects programming a non-erased half-word",
            header,
        )


if __name__ == "__main__":
    unittest.main()
