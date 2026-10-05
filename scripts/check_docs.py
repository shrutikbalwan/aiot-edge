"""Reject obsolete platform/security claims outside historical issue records."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

FORBIDDEN = (
    r"mqtt\.googleapis\.com",
    r"Google Cloud IoT Core",
    r"skip_cert_common_name_check\s*=\s*true",
    r"Cortex-M4",
    r"armv7e-m",
    r"TrustZone",
    r"100% (?:software |code-level )?complete",
)
EXCLUDED = {
    "docs/KNOWN_ISSUES.md",
    "docs/archive/LEGACY_PROTOTYPE.md",
    "scripts/check_docs.py",
}


def main() -> int:
    files = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard"], check=True, capture_output=True,
                           text=True).stdout.splitlines()
    failures: list[str] = []
    for relative in files:
        if relative in EXCLUDED:
            continue
        path = Path(relative)
        try:
            content = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for term in FORBIDDEN:
            if re.search(term, content, re.IGNORECASE):
                failures.append(f"{relative}: forbidden obsolete claim matches {term!r}")
    if failures:
        print("\n".join(failures))
        return 1
    print("documentation consistency: passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
