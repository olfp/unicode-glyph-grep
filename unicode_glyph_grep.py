#!/usr/bin/env python3
"""Search ordinary text in files that use Unicode math glyphs.

This script normalizes the line being searched with NFKC so that glyphs like
`𝐩𝐫𝐨𝐜` and `𝑟𝑒𝑠𝑢𝑙𝑡` match plain text patterns like `proc` and `result`.
It prints the original source line, not the normalized text.
"""

from __future__ import annotations

import argparse
import re
import sys
import unicodedata
from pathlib import Path


def normalize_for_search(text: str) -> str:
    """Normalize compatibility characters so math glyphs match ASCII text."""
    return unicodedata.normalize("NFKC", text)


def search_files(paths, pattern, *, ignore_case=False, invert=False, line_numbers=False, quiet=False):
    flags = 0
    if ignore_case:
        flags |= re.IGNORECASE

    regex = re.compile(pattern, flags)
    matched_any = False

    file_list = []
    stdin_requested = False

    for path in paths:
        if path == "-":
            stdin_requested = True
            continue
        p = Path(path)
        if p.is_dir():
            # Recursively include regular files. This keeps the tool useful for
            # file trees without requiring a separate shell pipeline.
            file_list.extend(sorted(p.rglob("*")))
        else:
            file_list.append(p)

    # Remove directories and non-files for predictable behavior.
    file_list = [p for p in file_list if p.is_file()]

    if stdin_requested:
        file_list.append(Path("-"))

    for file_path in file_list:
        if file_path == Path("-"):
            for line in sys.stdin:
                line_text = line.rstrip("\n")
                normalized = normalize_for_search(line_text)
                match = bool(regex.search(normalized))
                if invert:
                    match = not match
                if match:
                    matched_any = True
                    if not quiet:
                        if line_numbers:
                            print(f"{line_text}")
                        else:
                            print(line_text)
            continue

        with open(file_path, "r", encoding="utf-8", errors="surrogateescape") as handle:
            for lineno, raw_line in enumerate(handle, 1):
                line_text = raw_line.rstrip("\n")
                normalized = normalize_for_search(line_text)
                match = bool(regex.search(normalized))
                if invert:
                    match = not match
                if match:
                    matched_any = True
                    if not quiet:
                        prefix = ""
                        if line_numbers:
                            prefix = f"{lineno}:"
                        if len(file_list) > 1 and file_path != Path("-"):
                            print(f"{file_path}:{prefix}{line_text}")
                        else:
                            print(f"{prefix}{line_text}")

    return 0 if matched_any else 1


def build_parser():
    parser = argparse.ArgumentParser(
        description=(
            "Search ordinary text patterns against files that contain Unicode "
            "math glyphs. Searches normalize text using NFKC while printing the "
            "original source line."
        )
    )
    parser.add_argument("pattern", nargs="?", help="Regular expression to search for")
    parser.add_argument("paths", nargs="*", help="Files or directories to search")
    parser.add_argument("-i", "--ignore-case", action="store_true", help="Ignore case")
    parser.add_argument("-n", "--line-number", action="store_true", help="Print line numbers")
    parser.add_argument("-v", "--invert-match", action="store_true", help="Invert match")
    parser.add_argument("-q", "--quiet", action="store_true", help="Suppress output and only return exit status")
    parser.add_argument("-r", "--recursive", action="store_true", help="Recursively search directories")
    parser.add_argument("-e", "--regexp", dest="regexp", help="Pattern supplied separately from the file list")
    return parser


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.regexp is not None:
        pattern = args.regexp
    elif args.pattern is not None:
        pattern = args.pattern
    else:
        parser.error("a pattern is required")

    paths = args.paths if args.paths else ["-"]

    if args.recursive:
        # The recursive switch is only meaningful for directories; files are still
        # accepted in the same way as the shell grep command.
        if paths == ["-"]:
            pass
        else:
            expanded_paths = []
            for p in paths:
                candidate = Path(p)
                if candidate.is_dir():
                    expanded_paths.extend(sorted(candidate.rglob("*")))
                else:
                    expanded_paths.append(candidate)
            paths = [str(p) for p in expanded_paths]

    exit_code = search_files(
        paths,
        pattern,
        ignore_case=args.ignore_case,
        invert=args.invert_match,
        line_numbers=args.line_number,
        quiet=args.quiet,
    )
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
