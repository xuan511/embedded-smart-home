#!/usr/bin/env python3
"""Build the host-only controller fixture; this never builds/flashes firmware."""
from pathlib import Path
import os
import shlex
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
cc = shlex.split(os.environ["CC"]) if os.environ.get("CC") else [shutil.which("cc") or shutil.which("gcc") or "cc"]
output = root / ".test-deps" / ("bridge_fixture.exe" if os.name == "nt" else "bridge_fixture")
sources = ["components/home_core/home_core.c", "components/home_link/home_link.c",
           "components/home_remote/home_remote.c", "components/home_gateway/home_gateway.c",
           "stm32/src/stm32_controller.c", ".test-deps/cjson/cJSON.c", "tests/bridge_fixture.c"]
includes = ["components/home_core/include", "components/home_link/include", "components/home_remote/include",
            "components/home_gateway/include", "stm32/include", ".test-deps/cjson"]
subprocess.run(cc + ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic"] +
               [arg for p in includes for arg in ("-I", str(root / p))] +
               [str(root / p) for p in sources] + ["-lm", "-o", str(output)], check=True)
print("Built HOST TEST fixture:", output)
