#!/usr/bin/env python3
"""Fetch the exact upstream cJSON 1.7.19 used by the managed IDF component."""
from pathlib import Path
import hashlib
import urllib.request

root = Path(__file__).resolve().parents[1] / ".test-deps/cjson"
hashes = {
    "cJSON.c": "298581a04a36c0165da4b0aade235c23088cb2faa58651d720ea2f3706ed0b0d",
    "cJSON.h": "25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee",
    "LICENSE": "a36dda207c36db5818729c54e7ad4e8b0c6fba847491ba64f372c1a2037b6d5c",
}
root.mkdir(parents=True, exist_ok=True)
for name, digest in hashes.items():
    file = root / name
    data = file.read_bytes() if file.exists() else urllib.request.urlopen(
        "https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.19/" + name, timeout=60).read()
    if hashlib.sha256(data).hexdigest() != digest:
        raise SystemExit("cJSON checksum mismatch: " + name)
    file.write_bytes(data)
print("cJSON 1.7.19 test dependency: SHA256 verified")
