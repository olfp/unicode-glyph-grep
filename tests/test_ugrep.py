import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "ugrep"


def build_binary():
    result = subprocess.run(
        ["make", "-C", str(ROOT), "ugrep"],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            "Failed to build ugrep:\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )


class UnicodeGlyphGrepTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build_binary()

    def run_tool(self, *args):
        return subprocess.run(
            [str(BINARY), *args],
            capture_output=True,
            text=True,
            check=False,
        )

    def test_matches_proc_in_glyphs(self):
        result = self.run_tool("proc", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("𝐩𝐫𝐨𝐜", result.stdout)

    def test_line_numbers_in_unicode_mode(self):
        result = self.run_tool("-n", "proc", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertRegex(result.stdout, r"(?m)^17:.*𝐩𝐫𝐨𝐜")

    def test_matches_value_in_italic_glyphs(self):
        result = self.run_tool("value", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("𝑣𝑎𝑙𝑢𝑒", result.stdout)

    def test_case_insensitive_match(self):
        result = self.run_tool("-i", "PROC", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("𝐩𝐫𝐨𝐜", result.stdout)

    def test_with_filename_option(self):
        result = self.run_tool("-H", "proc", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("demo.u68:", result.stdout)

    def test_count_mode(self):
        result = self.run_tool("-c", "value", str(ROOT / "demo.u68"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "2")

    def test_max_count(self):
        result = self.run_tool(
            "-m",
            "1",
            "proc",
            str(ROOT / "demo.u68"),
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("𝐩𝐫𝐨𝐜", result.stdout)

    def test_no_unicode_short_option(self):
        result = self.run_tool(
            "-t",
            "proc",
            str(ROOT / "demo.u68"),
        )
        self.assertEqual(result.returncode, 1)

    def test_no_unicode_long_option(self):
        result = self.run_tool(
            "--no-unicode",
            "proc",
            str(ROOT / "demo.u68"),
        )
        self.assertEqual(result.returncode, 1)

    def test_no_match_returns_1(self):
        result = self.run_tool(
            "zzzz-not-present",
            str(ROOT / "demo.u68"),
        )
        self.assertEqual(result.returncode, 1)

    def test_help(self):
        result = self.run_tool("--help")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--unicode", result.stdout)
        self.assertIn("--no-unicode", result.stdout)


if __name__ == "__main__":
    unittest.main()
