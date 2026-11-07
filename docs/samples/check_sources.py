#!/usr/bin/env python3
"""Every sample file must have a row in SOURCES.md.

Usage: check_sources.py [samples dir]   (default: the folder of this script)
Exit 0 if every file's relative path occurs in SOURCES.md as a whole path, else 1, naming each one.
"""
import re
import sys
from pathlib import Path


def main() -> int:
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent
    sources = (root / "SOURCES.md").read_text(encoding="utf-8")
    missing = []
    for path in sorted(root.rglob("*")):
        if not path.is_file() or "__pycache__" in path.parts or path.suffix in (".md", ".py"):
            continue
        rel = path.relative_to(root).as_posix()
        if not re.search(r"(?<![A-Za-z0-9_./-])" + re.escape(rel) + r"(?![A-Za-z0-9_./-])", sources):
            missing.append(rel)
    for rel in missing:
        print(f"no source in SOURCES.md: {rel}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
