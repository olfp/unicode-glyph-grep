import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "ugrep"


def run_tool(*args):
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        capture_output=True,
        text=True,
        check=False,
    )


class UnicodeGlyphGrepTests(unittest.TestCase):
    def test_matches_proc_in_glyphs(self):
        result = run_tool("proc", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("𝐩𝐫𝐨𝐜", result.stdout)

    def test_matches_value_in_italic_glyphs(self):
        result = run_tool("value", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("𝑣𝑎𝑙𝑢𝑒", result.stdout)

    def test_case_insensitive_match(self):
        result = run_tool("-i", "PROC", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("𝐩𝐫𝐨𝐜", result.stdout)

    def test_no_match_returns_1(self):
        result = run_tool("zzzz-not-present", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 1)


if __name__ == "__main__":
    unittest.main()
