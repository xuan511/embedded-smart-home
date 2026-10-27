#!/usr/bin/env python3
"""Read-only check of the exact documented S31 SDK baseline; never print secrets."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

EXPECTED_IDF = "d35ea023029363d97c42565f85e0db23486f26b2"
EXPECTED_GCC = "16.1.0"
errors = []
print(f"Python: {sys.version.split()[0]}")
for tool in ("git", "cmake", "ninja", "riscv32-esp-elf-gcc"):
    found = shutil.which(tool)
    print(f"{tool}: {'OK' if found else 'MISSING'}")
    if not found:
        errors.append(tool)

def git_at(root, *args):
    return subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True, encoding="utf-8")

idf = os.environ.get("IDF_PATH")
idf_script = Path(idf) / "tools/idf.py" if idf else None
# Official PowerShell activation may expose idf.py as a function, which which()
# cannot see. The SDK script can also be invoked with the activated Python.
has_idf = bool(shutil.which("idf.py") or (idf_script and idf_script.is_file()))
print(f"idf.py (command or SDK script): {'OK' if has_idf else 'MISSING'}")
if not has_idf:
    errors.append("idf.py")
if idf and shutil.which("git"):
    root = Path(idf)
    supported = (root / "components/soc/esp32s31").is_dir()
    print(f"ESP32-S31 SDK sources: {'OK' if supported else 'MISSING'}")
    if not supported:
        errors.append("esp32s31 target sources")
    revision = git_at(root, "rev-parse", "HEAD")
    actual = revision.stdout.strip()
    print(f"SDK commit: {actual if revision.returncode == 0 else 'UNAVAILABLE'}")
    if revision.returncode != 0 or actual != EXPECTED_IDF:
        errors.append("SDK differs from documented commit " + EXPECTED_IDF)
    changes = git_at(root, "diff", "--name-only", "--ignore-submodules=all", "HEAD")
    if changes.returncode != 0 or changes.stdout.strip():
        errors.append("SDK tracked sources are modified or cannot be checked")
    modules = git_at(root, "submodule", "status", "--recursive")
    bad = [line for line in modules.stdout.splitlines() if line[:1] in ("-", "+", "U")]
    print(f"SDK recursive submodules: {'OK' if modules.returncode == 0 and not bad else 'INCOMPLETE/DIFFERENT'}")
    if modules.returncode != 0 or bad:
        errors.append("run git -c core.longpaths=true submodule update --init --recursive in the fixed SDK")
else:
    errors.append("IDF_PATH / activate the fixed SDK")
if os.environ.get("IDF_SKIP_CHECK_SUBMODULES") == "1":
    errors.append("IDF_SKIP_CHECK_SUBMODULES=1 bypasses the full official setup check; unset it")
if shutil.which("riscv32-esp-elf-gcc"):
    result = subprocess.run(["riscv32-esp-elf-gcc", "-dumpfullversion"], capture_output=True, text=True)
    version = result.stdout.strip()
    print(f"RISC-V GCC version: {version}")
    if result.returncode != 0 or version != EXPECTED_GCC:
        errors.append("expected the fixed SDK's GCC " + EXPECTED_GCC)
if errors:
    print("Baseline check FAILED:\n- " + "\n- ".join(errors))
    sys.exit(1)
print("Baseline check PASS. This checks setup only; it is not a hardware test.")
print("Next: idf.py --preview set-target esp32s31; idf.py --preview build")
