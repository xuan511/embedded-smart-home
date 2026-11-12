#!/usr/bin/env python3
"""Read-only REAL-device JSONL capture; never operates loads or proves motion.

Standard-library API basis: Python 3.11 json, ipaddress, urllib.request and
time.monotonic documentation. Acceptance criteria below are project design.
"""
import argparse
from collections import Counter
from datetime import datetime, timezone
import getpass
import ipaddress
import json
import math
from pathlib import Path
import time
import urllib.error
import urllib.request

LAN_RANGES = tuple(ipaddress.IPv4Network(n) for n in ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16"))


def lan_address(text):
    try:
        ip = ipaddress.IPv4Address(text)
    except ipaddress.AddressValueError as error:
        raise argparse.ArgumentTypeError("Enter Korvo IPv4, without http:// or a port") from error
    if not any(ip in network for network in LAN_RANGES):
        raise argparse.ArgumentTypeError("Use the real Korvo LAN IP; localhost/public/documentation IPs are excluded")
    return str(ip)


def utc_now():
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def validate_state(state, require_mqtt=False, transport_ms=0):
    """Evaluate reports only, not sensor accuracy or physical actuator behavior."""
    if not isinstance(state, dict):
        return ["not_an_object"]
    reasons = []
    for key, expected in (("schema", 1), ("stage", 4)):
        if type(state.get(key)) is not int or state[key] != expected:
            reasons.append(key)
    for key in ("online", "synced", "outputs_known", "wifi"):
        if state.get(key) is not True:
            reasons.append(key)
    for key in ("simulated", "stale", "temperature_fault", "humidity_fault", "light_fault", "physical_feedback"):
        if state.get(key) is not False:
            reasons.append(key)
    if state.get("source") != "sensor":
        reasons.append("source")
    if state.get("mode") not in ("auto", "manual"):
        reasons.append("mode")
    if any(type(state.get(k)) is not bool for k in ("light_on", "fan_on")):
        reasons.append("output_types")
    limits = {"temperature_c": (-40, 125), "humidity_pct": (0, 100), "illuminance_lux": (0, 100000)}
    for key, (low, high) in limits.items():
        number = state.get(key)
        if type(number) not in (int, float) or not low <= number <= high or not math.isfinite(number):
            reasons.append(key)
    age, stale = state.get("sample_age_ms"), state.get("stale_ms")
    if type(age) is not int or type(stale) is not int or not 1000 <= stale <= 60000 or not 0 <= age + transport_ms < stale:
        reasons.append("sample_age")
    link_age = state.get("link_age_ms")
    if type(link_age) is not int or not 0 <= link_age + transport_ms < 3000:
        reasons.append("link_age")
    if type(state.get("uptime_ms")) is not int or not 0 <= state["uptime_ms"] <= 2**64 - 1:
        reasons.append("uptime")
    if require_mqtt and state.get("mqtt") is not True:
        reasons.append("mqtt")
    return reasons


class CaptureStats:
    def __init__(self):
        self.samples = self.failures = self.observed_resets = 0
        self.last_uptime = None
        self.reasons = Counter()

    def observe(self, record):
        reasons = record.setdefault("reasons", [])
        state = record.get("state")
        uptime = state.get("uptime_ms") if isinstance(state, dict) else None
        if type(uptime) is int and uptime >= 0:
            if self.last_uptime is not None:
                if uptime < self.last_uptime:
                    self.observed_resets += 1
                    reasons.append("observed_uptime_reset")
                elif uptime == self.last_uptime:
                    reasons.append("uptime_not_advancing")
            self.last_uptime = uptime
        record["ok"] = not reasons
        self.samples += 1
        self.failures += bool(reasons)
        self.reasons.update(reasons)


def finite_float(text):
    number = float(text)
    if not math.isfinite(number):
        raise ValueError("Non-finite JSON number")
    return number


def invalid_constant(_):
    raise ValueError("Non-standard JSON constant")


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None  # Do not silently switch a fixed-device capture to another host.


def read_state(ip, token, opener):
    request = urllib.request.Request("http://" + ip + "/api/state", headers={"Authorization": "Bearer " + token})
    with opener.open(request, timeout=2.5) as response:
        if response.status != 200:
            raise ValueError("Unexpected HTTP status")
        body = response.read(4097)
        if len(body) > 4096:
            raise ValueError("Status response exceeds 4096 bytes")
    return json.loads(body, parse_float=finite_float, parse_constant=invalid_constant)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ip", type=lan_address, required=True)
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-mqtt", action="store_true", help="Also require the reported MQTT connection")
    args = parser.parse_args()
    if args.seconds < 1:
        parser.error("--seconds must be positive")
    if args.output.exists():
        parser.error("Choose a new output path; existing evidence is never overwritten")
    token = getpass.getpass("Korvo API token (not logged): ")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # This local-device check deliberately does not use system HTTP proxies.
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())
    stats = CaptureStats()
    start = next_poll = time.monotonic()
    interrupted = False
    with args.output.open("x", encoding="utf-8") as file:
        def write(record):
            file.write(json.dumps(record, ensure_ascii=False, allow_nan=False) + "\n")
            file.flush()
        write({"event": "start", "utc": utc_now(), "device_ip": args.ip, "duration_seconds": args.seconds,
               "require_mqtt": args.require_mqtt, "nominal_period_seconds": 1,
               "validation_age_includes_http_elapsed": True})
        try:
            while time.monotonic() - start < args.seconds:
                record = {"utc": utc_now(), "elapsed_s": round(time.monotonic() - start, 3)}
                request_start = time.monotonic()
                try:
                    state = read_state(args.ip, token, opener)
                    transport_ms = (time.monotonic() - request_start) * 1000
                    record.update(state=state, reasons=validate_state(state, args.require_mqtt, transport_ms))
                except Exception as error:
                    record["reasons"] = [type(error).__name__]
                    if isinstance(error, urllib.error.HTTPError):
                        record["http_status"] = error.code
                record["http_response_ms"] = round((time.monotonic() - request_start) * 1000, 3)
                stats.observe(record)
                write(record)
                next_poll = max(next_poll + 1, time.monotonic())
                time.sleep(max(0, min(next_poll, start + args.seconds) - time.monotonic()))
        except KeyboardInterrupt:
            interrupted = True
        summary = {"event": "end", "utc": utc_now(), "actual_seconds": round(time.monotonic() - start, 3),
                   "samples": stats.samples, "failures": stats.failures, "observed_korvo_resets": stats.observed_resets,
                   "failure_reasons": dict(stats.reasons), "interrupted": interrupted,
                   "physical_feedback_verified": False, "stm32_resets_measured": False,
                   "sensor_accuracy_verified": False}
        write(summary)
    print(json.dumps(summary, ensure_ascii=False))
    print("Reports only; sensor accuracy and actual fan motion require separate physical observations.")
    return 1 if interrupted or stats.failures or not stats.samples else 0


if __name__ == "__main__":
    raise SystemExit(main())
