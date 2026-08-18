from __future__ import annotations

import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]


class SecureAbReleaseIntegrationTest(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (REPO / relative).read_text(encoding="utf-8")

    def test_release_version_matches_acceptance_directory(self) -> None:
        version = self.read("VERSION").strip()
        self.assertEqual(version, "2.1.0")
        self.assertTrue((REPO / f"docs/acceptance/v{version}/RESULT.md").is_file())
        self.assertTrue((REPO / "docs/RELEASE_V2.1.0.md").is_file())

    def test_imx_build_includes_secure_host(self) -> None:
        script = self.read("scripts/build_all.sh")
        self.assertIn(
            'make -C "$BASE_DIR/linux/can_ota_host_ab_secure" CC="$IMX_CC"',
            script,
        )

    def test_target_bundle_includes_secure_host_and_acceptance(self) -> None:
        script = self.read("scripts/package_target.sh")
        self.assertIn(
            'linux/can_ota_host_ab_secure/stm32_can_ota_ab_secure_host',
            script,
        )
        self.assertIn('docs/acceptance/v$VERSION', script)
        self.assertIn("docs/RELEASE_V2.1.0.md", script)
        self.assertIn("docs/RELEASE_V2.1.0_RC1.md", script)

    def test_installer_uses_separate_persistent_directory(self) -> None:
        script = self.read("scripts/install_target.sh")
        destination = (
            '$APP_DIR/linux/can_ota_host_ab_secure/'
            'stm32_can_ota_ab_secure_host'
        )
        self.assertIn(destination, script)
        self.assertIn('$APP_DIR/linux/can_ota_host/stm32_can_ota_host', script)

    def test_production_host_gates_acceptance_override(self) -> None:
        source = self.read(
            "linux/can_ota_host_ab_secure/stm32_can_ota_host.c"
        )
        self.assertIn("#ifdef CAN_OTA_ACCEPTANCE_TEST", source)
        self.assertNotIn("CAN_OTA_ACCEPTANCE_TEST", self.read(
            "linux/can_ota_host_ab_secure/Makefile"
        ))

    def test_private_key_and_ota_packages_are_not_packaged(self) -> None:
        script = self.read("scripts/package_target.sh")
        self.assertNotIn("dist-ota-dev", script)
        self.assertNotIn("private.pem", script)
        self.assertNotIn("OTA_KEY_PASSWORD", script)

    def test_acceptance_archive_verifies_evidence_first(self) -> None:
        script = self.read("scripts/package_acceptance.sh")
        verify = "tr -d '\\r' < EVIDENCE_SHA256SUMS.txt | sha256sum -c -"
        archive = 'tar -czf "$ARCHIVE"'
        self.assertIn("manifest_count", script)
        self.assertIn('EVIDENCE_VERSION="${EVIDENCE_VERSION:-}"', script)
        self.assertIn("EVIDENCE_SOURCE_VERSION", script)
        self.assertEqual(
            self.read("docs/acceptance/v2.1.0/EVIDENCE_SOURCE_VERSION").strip(),
            "2.1.0-rc.1",
        )
        self.assertIn(verify, script)
        self.assertIn(archive, script)
        self.assertLess(script.index(verify), script.index(archive))

    def test_imx_build_and_bundle_include_sensor_diagnostic(self) -> None:
        build = self.read("scripts/build_all.sh")
        package = self.read("scripts/package_target.sh")
        installer = self.read("scripts/install_target.sh")
        self.assertIn('linux/kernel_drivers/imx6ull_sensors"', build)
        self.assertIn('user-test USER_CC="$IMX_CC"', build)
        self.assertIn("linux/sensor_diag/sensor_smoke_test", build)
        self.assertIn("linux/sensor_diag/sensor_smoke_test", package)
        self.assertIn("linux/sensor_diag/sensor_smoke_test", installer)


if __name__ == "__main__":
    unittest.main()
