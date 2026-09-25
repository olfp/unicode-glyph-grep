import subprocess
import sys
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "unicode_glyph_grep.py"

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
        self.assertIn("demo.u68", result.stdout)

    def test_matches_result_in_italic_glyphs(self):
        result = run_tool("result", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("𝑟𝑒𝑠𝑢𝑙𝑡", result.stdout)

    def test_case_insensitive_match(self):
        result = run_tool("-i", "PROCEDURE", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 1)

    def test_no_match_returns_1(self):
        result = run_tool("zzzz-not-present", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 1)


if __name__ == "__main__":
    unittest.main()
