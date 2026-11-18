# Copyright 2026 TM-59
"""Local build output budget; no third-party packages (Python 3.14)."""

import sys
from pathlib import Path

LIMIT_BYTES = 10_000_000


class DownloadTooLargeError(SystemExit):
    """The files in dist/ add up to more than the initial-load budget."""

    def __init__(self) -> None:
        """Exit with the limit in the message."""
        super().__init__(f"Initial load exceeds {LIMIT_BYTES:,} B")


files = sorted(p for p in Path("dist").rglob("*") if p.is_file())
total = sum(p.stat().st_size for p in files)
for p in files:
    sys.stdout.write(f"{p}: {p.stat().st_size:,} B\n")
sys.stdout.write(f"Initial load: {total:,} B / {LIMIT_BYTES:,} B\n")
if total > LIMIT_BYTES:
    raise DownloadTooLargeError
