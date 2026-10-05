from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path


def test_credential_policy_native() -> None:
    compiler = shutil.which("gcc")
    assert compiler, "gcc is required for host C tests"
    executable = Path("tests/runtime/credential_policy_test.exe")
    command = [
        compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Icomponents/aiot_common/include",
        "components/aiot_common/credential_policy.c",
        "tests/host/test_credential_policy.c",
        "-o",
        str(executable),
    ]
    if os.name != "nt":
        command.insert(5, "-fsanitize=undefined")
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], check=True)
