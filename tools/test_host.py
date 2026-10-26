#!/usr/bin/env python3
"""Compile and run the actual portable C controller; no ESP-IDF needed."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="home-test-") as work:
    executable = Path(work) / ("test_core.exe" if os.name == "nt" else "test_core")
    command = shlex.split(os.environ.get("CC", "cc")) + [
        "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-g",
        "-I", str(root / "components/home_core/include"),
        str(root / "components/home_core/home_core.c"),
        str(root / "tests/test_core.c"), "-lm", "-o", str(executable),
    ]
    if os.environ.get("SANITIZE") == "1":
        command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], check=True)
