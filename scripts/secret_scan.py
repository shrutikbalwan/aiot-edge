"""Small dependency-free secret-pattern gate used locally and in CI."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

PATTERNS = {
    "private key": re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    "AWS access key": re.compile(r"AKIA[0-9A-Z]{16}"),
    "GitHub token": re.compile(r"gh[pousr]_[A-Za-z0-9_]{30,}"),
    "generic password assignment": re.compile(r"(?i)(?:password|passwd)\s*[=:]\s*['\"][^'\"]{8,}['\"]"),
}


def main() -> int:
    files = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard"], check=True, capture_output=True,
                           text=True).stdout.splitlines()
    findings: list[str] = []
    for relative in files:
        path = Path(relative)
        try:
            content = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for name, pattern in PATTERNS.items():
            for match in pattern.finditer(content):
                line = content.count("\n", 0, match.start()) + 1
                findings.append(f"{relative}:{line}: {name}")
    if findings:
        print("\n".join(findings))
        return 1
    print(f"secret scan: checked {len(files)} versioned or untracked files; no configured pattern matched")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
