"""Release packaging contracts, using synthetic firmware and a stub esptool."""

import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from release_artifacts import build_identity, package_release


class BuildEnvironment(dict):
    def __init__(self, root):
        super().__init__(BUILD_FLAGS=[], PIOENV="waveshare-relay-6ch")
        self.root = root
        self.defines = [["APP_VERSION", '\\"0.6.3-beta.1\\"'], ("APP_NAME", '"ESP32-Sveltekit"')]
        self.board = {"build.mcu": "esp32s3", "upload.flash_size": "8MB", "build.flash_mode": "qio"}
        self["FLASH_EXTRA_IMAGES"] = [("0x0", str(root / "boot loader.bin"))]

    def ParseFlags(self, flags):
        return {"CPPDEFINES": self.defines}

    def BoardConfig(self):
        return self.board

    def Flatten(self, items):
        return [value for item in items for value in item]

    def subst(self, value):
        replacements = {
            "$PROJECT_DIR": str(self.root), "$BUILD_DIR": str(self.root / "build input"),
            "${PROGNAME}": "firmware", "$PYTHONEXE": "C:/Python tools/python.exe",
            "$OBJCOPY": "C:/ESP tools/esptool.py", "$ESP32_APP_OFFSET": "0x10000",
        }
        for key, replacement in replacements.items():
            value = value.replace(key, replacement)
        return value


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="release tests ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.env = BuildEnvironment(self.root)
        inputs = self.root / "build input"
        inputs.mkdir()
        (inputs / "firmware.bin").write_bytes(b"synthetic OTA image")
        (inputs / "firmware.elf").write_bytes(b"synthetic debug symbols")
        self.stem = "edge_switch_actuator_waveshare-relay-6ch_0.6.3-beta.1"

    def merge(self, command, **kwargs):
        self.assertTrue(kwargs["check"])
        self.assertEqual(kwargs["cwd"], str(self.root))
        self.assertEqual(command[0], "C:/Python tools/python.exe")
        self.assertIn(str(self.root / "boot loader.bin"), command)
        self.assertEqual(command[-2], "0x10000")
        self.assertEqual(Path(command[-1]), self.root / "build input/firmware.bin")
        self.assertEqual(command[command.index("--flash-mode") + 1], "dio")
        Path(command[command.index("-o") + 1]).write_bytes(b"synthetic merged image")

    def test_project_board_version_outputs_match_current_build_and_recover_deleted_export(self):
        with patch("release_artifacts.subprocess.run", side_effect=self.merge):
            package_release([], [], self.env)
            output = self.root / "buildRelease"
            self.assertEqual({p.name for p in output.iterdir()}, {
                f"{self.stem}_ota.bin", f"{self.stem}_ota.md5",
                f"{self.stem}_webflash.bin", f"{self.stem}.elf",
            })
            ota = output / f"{self.stem}_ota.bin"
            self.assertEqual(ota.read_bytes(), b"synthetic OTA image")
            self.assertEqual((output / f"{self.stem}.elf").read_bytes(), b"synthetic debug symbols")
            self.assertEqual((output / f"{self.stem}_ota.md5").read_text(), hashlib.md5(ota.read_bytes()).hexdigest())
            ota.unlink()
            package_release([], [], self.env)
            self.assertTrue(ota.exists())

    def test_failed_merge_preserves_previous_release_and_cleans_staging(self):
        with patch("release_artifacts.subprocess.run", side_effect=self.merge):
            package_release([], [], self.env)
        output = self.root / "buildRelease"
        before = {p.name: p.read_bytes() for p in output.iterdir()}
        (self.root / "build input/firmware.bin").write_bytes(b"new firmware")
        with patch("release_artifacts.subprocess.run", side_effect=subprocess.CalledProcessError(1, "esptool")):
            with self.assertRaises(subprocess.CalledProcessError):
                package_release([], [], self.env)
        self.assertEqual(before, {p.name: p.read_bytes() for p in output.iterdir()})

    def test_environment_outputs_do_not_collide(self):
        with patch("release_artifacts.subprocess.run", side_effect=self.merge):
            package_release([], [], self.env)
            self.env["PIOENV"] = "esp32-s3-devkitc-1"
            package_release([], [], self.env)
        self.assertEqual(len(list((self.root / "buildRelease").iterdir())), 8)

    def test_missing_or_unsafe_version_is_rejected(self):
        for defines in ([], [["APP_VERSION", '"../escape"'], ["APP_NAME", "app"]]):
            self.env.defines = defines
            with self.assertRaises(ValueError):
                build_identity(self.env)


if __name__ == "__main__":
    unittest.main()
