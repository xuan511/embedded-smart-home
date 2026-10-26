#!/usr/bin/env python3
"""Read-only SDK sanity check; never prints environment variables or credentials."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

idf = os.environ.get("IDF_PATH")
errors = []
print(f"Python: {sys.version.split()[0]}")
for tool in ("git", "idf.py", "cmake", "ninja", "riscv32-esp-elf-gcc"):
    found = shutil.which(tool)
    print(f"{tool}: {'OK' if found else 'MISSING'}")
    if not found:
        errors.append(tool)
if idf:
    root = Path(idf)
    supported = (root / "components/soc/esp32s31").is_dir()
    print(f"ESP32-S31 SDK sources: {'OK' if supported else 'MISSING'}")
    if not supported:
        errors.append("esp32s31 target")
    result = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], capture_output=True, text=True)
    if result.returncode == 0:
        print(f"SDK commit: {result.stdout.strip()}")
else:
    errors.append("IDF_PATH / export.sh or export.ps1")
if errors:
    print("Activate an S31-capable ESP-IDF terminal. Missing: " + ", ".join(errors))
    sys.exit(1)
print("Environment found. Next: idf.py --preview set-target esp32s31 && idf.py --preview build")
