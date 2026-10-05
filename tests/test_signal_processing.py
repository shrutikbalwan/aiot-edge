from __future__ import annotations

import shutil
import subprocess
import os
from pathlib import Path


def test_signal_processing_native() -> None:
    compiler = shutil.which("gcc")
    assert compiler, "gcc is required for host C tests"
    executable = Path("tests/runtime/signal_test.exe")
    command = [
        compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Icomponents/sensors/include",
        "components/sensors/signal_processing.c",
        "tests/host/test_signal.c",
        "-o",
        str(executable),
    ]
    if os.name != "nt":
        command.insert(5, "-fsanitize=undefined")
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], check=True)
