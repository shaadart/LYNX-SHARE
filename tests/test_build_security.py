"""Exercise checksum failures without downloading or executing packaging tools."""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/fetch-appimagetool.sh"


class AppImageToolTests(unittest.TestCase):
    def test_corrupt_cached_tool_is_rejected_without_execution(self):
        with tempfile.TemporaryDirectory() as folder:
            tool = Path(folder) / "appimagetool"
            marker = Path(folder) / "executed"
            tool.write_text(f"#!/bin/sh\ntouch '{marker}'\n")
            tool.chmod(0o755)
            result = subprocess.run(["bash", str(SCRIPT), str(tool)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"FAILED", result.stdout)
            self.assertFalse(marker.exists())

    def test_corrupt_download_is_not_installed(self):
        with tempfile.TemporaryDirectory() as folder:
            fake_bin = Path(folder) / "bin"
            fake_bin.mkdir()
            curl = fake_bin / "curl"
            curl.write_text('#!/bin/sh\nprintf "corrupt download" > "$3"\n')
            curl.chmod(0o755)
            tool = Path(folder) / "output/appimagetool"
            result = subprocess.run(["bash", str(SCRIPT), str(tool)], capture_output=True,
                                    env={**os.environ, "PATH": str(fake_bin) + os.pathsep + os.environ["PATH"]})
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"FAILED", result.stdout)
            self.assertFalse(tool.exists())


@unittest.skipUnless(shutil.which("cmake"), "cmake is required to exercise the JUCE patch")
class JucePatchTests(unittest.TestCase):
    def test_existing_mnemonic_patch_is_upgraded_once(self):
        with tempfile.TemporaryDirectory() as folder:
            popup = Path(folder) / "modules/juce_gui_basics/menus/juce_PopupMenu.cpp"
            popup.parent.mkdir(parents=True)
            popup.write_text("// LYNX patch (round 170a): menu accelerator letters\n"
                             "void mnemonic() { disableTimerUntilMouseMoves(); }\n")
            command = ["cmake", f"-DJUCE_DIR={folder}", "-P", str(SCRIPT.parents[1] / "cmake/LynxPatchJuce.cmake")]
            subprocess.run(command, check=True, capture_output=True)
            patched = popup.read_bytes()
            self.assertNotIn(b"disableTimerUntilMouseMoves();", patched)
            self.assertIn(b"disableMouseMovesOnMenuAndAncestors();", patched)
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual(popup.read_bytes(), patched)


if __name__ == "__main__":
    unittest.main()
