#!/usr/bin/env python3
"""Read-only REAL-device status capture. Never substitutes simulated values."""
import argparse
import getpass
import ipaddress
import json
from pathlib import Path
import time
import urllib.request

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--ip", required=True)
parser.add_argument("--seconds", type=int, default=60)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
address = ipaddress.IPv4Address(args.ip)
if not address.is_private or address.is_loopback or args.seconds < 1:
    parser.error("Use the physical Korvo LAN IPv4 address and a positive duration")
token = getpass.getpass("Korvo API token (not logged): ")
args.output.parent.mkdir(parents=True, exist_ok=True)
if args.output.exists():
    raise SystemExit("Choose a new log path; existing evidence is never overwritten")
start = time.monotonic()
failures = samples = 0
with args.output.open("x", encoding="utf-8") as file:
    file.write(json.dumps({"event": "start", "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "duration_seconds": args.seconds}) + "\n")
    while time.monotonic() - start < args.seconds:
        record = {"elapsed_s": round(time.monotonic() - start, 3)}
        try:
            request = urllib.request.Request("http://" + str(address) + "/api/state", headers={"Authorization": "Bearer " + token})
            with urllib.request.urlopen(request, timeout=2.5) as response:
                state = json.loads(response.read(4096))
            ok = state.get("schema") == 1 and state.get("source") == "sensor" and state.get("stage") == 4 and \
                state.get("online") is True and state.get("synced") is True and state.get("stale") is False and \
                state.get("simulated") is False and all(isinstance(state.get(k), (float, int)) for k in ("temperature_c", "humidity_pct", "illuminance_lux"))
            record.update(ok=ok, state=state)
            failures += not ok
        except Exception as error:
            failures += 1; record.update(ok=False, error=type(error).__name__)
        samples += 1
        file.write(json.dumps(record, ensure_ascii=False) + "\n"); file.flush()
        time.sleep(1)
    file.write(json.dumps({"event": "end", "actual_seconds": round(time.monotonic() - start, 3), "samples": samples, "failures": failures, "physical_feedback_verified": False}) + "\n")
print(f"Captured {samples} status reports; failures={failures}; physical fan operation is NOT verified by this script")
raise SystemExit(1 if failures else 0)
