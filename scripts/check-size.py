"""Local build output budget; no third-party packages (Python 3.14)."""
from pathlib import Path

files = sorted(p for p in Path("dist").rglob("*") if p.is_file())
total = sum(p.stat().st_size for p in files)
for p in files:
    print(f"{p}: {p.stat().st_size:,} B")
print(f"Initial load: {total:,} B / 10,000,000 B")
if total > 10_000_000:
    raise SystemExit("Initial load exceeds budget")
