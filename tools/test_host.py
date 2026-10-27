#!/usr/bin/env python3
"""Compile/run the actual portable C controller, line protocol and STM32 endpoint."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
core = "components/home_core/home_core.c"
link = "components/home_link/home_link.c"
remote = "components/home_remote/home_remote.c"
stm = "stm32/src/stm32_controller.c"
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise SystemExit("Install a native host C compiler (for example MinGW-w64 GCC on Windows), or set CC. Arm/riscv cross compilers cannot run these host tests.")
# Discovered paths can contain spaces; an explicit CC may contain quoted arguments.
compiler_command = shlex.split(compiler) if os.environ.get("CC") else [compiler]
suites = {
    "test_core": [core, "tests/test_core.c"],
    "test_link": [link, "tests/test_link.c"],
    "test_stm32_controller": [core, link, stm, "tests/test_stm32_controller.c"],
    "test_remote": [core, link, remote, "tests/test_remote.c"],
    "test_integration": [core, link, remote, stm, "tests/test_integration.c"],
}
with tempfile.TemporaryDirectory(prefix="home-test-") as work:
    for suite, sources in suites.items():
        executable = Path(work) / (suite + (".exe" if os.name == "nt" else ""))
        command = compiler_command + [
            "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-g",
            "-I", str(root / "components/home_core/include"),
            "-I", str(root / "components/home_link/include"),
            "-I", str(root / "components/home_remote/include"),
            "-I", str(root / "stm32/include"),
        ] + [str(root / source) for source in sources] + ["-lm", "-o", str(executable)]
        if os.environ.get("SANITIZE") == "1":
            command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        subprocess.run(command, check=True)
        subprocess.run([str(executable)], check=True)
