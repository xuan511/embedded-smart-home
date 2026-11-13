"""Regression tests for the actual acceptance logger; these are HOST fixtures."""
import argparse
import importlib.util
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("live_check", Path(__file__).resolve().parents[1] / "tools/live_check.py")
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)


def valid_state():
    return {"schema": 1, "stage": 4, "online": True, "synced": True,
            "outputs_known": True, "wifi": True, "simulated": False, "stale": False,
            "temperature_fault": False, "humidity_fault": False, "light_fault": False,
            "physical_feedback": False, "source": "sensor", "mode": "auto",
            "light_on": False, "fan_on": True, "temperature_c": 30, "humidity_pct": 50,
            "illuminance_lux": 100, "sample_age_ms": 100, "stale_ms": 5000,
            "uptime_ms": 10000, "link_age_ms": 20, "mqtt": False}


class LiveCheckTests(unittest.TestCase):
    def test_valid_without_mqtt_and_explicit_mqtt_requirement(self):
        self.assertEqual(live.validate_state(valid_state()), [])
        self.assertEqual(live.validate_state(valid_state(), True), ["mqtt"])

    def test_bool_nonfinite_null_and_out_of_range_are_not_measurements(self):
        for value in (True, False, None, "30", float("nan"), float("inf"), -41, 126):
            with self.subTest(value=value):
                s = valid_state(); s["temperature_c"] = value
                self.assertIn("temperature_c", live.validate_state(s))

    def test_reported_ok_does_not_override_bad_age_or_fault(self):
        for age in (-1, 5000, True, "0"):
            s = valid_state(); s["sample_age_ms"] = age
            self.assertIn("sample_age", live.validate_state(s))
        s = valid_state(); s["temperature_fault"] = True
        self.assertIn("temperature_fault", live.validate_state(s))
        s = valid_state(); s.pop("stale_ms")
        self.assertIn("sample_age", live.validate_state(s))

    def test_test_simulation_wrong_stage_and_missing_fields_fail(self):
        for key, value in (("source", "test"), ("simulated", True), ("stage", 2),
                           ("stage", True), ("online", 1), ("outputs_known", False),
                           ("mode", "unknown"), ("fan_on", None)):
            s = valid_state(); s[key] = value
            self.assertTrue(live.validate_state(s), (key, value))
        self.assertTrue(live.validate_state({}))
        self.assertEqual(live.validate_state([]), ["not_an_object"])

    def test_transport_delay_cannot_make_an_old_report_fresh(self):
        s = valid_state(); s["sample_age_ms"] = 4500
        self.assertIn("sample_age", live.validate_state(s, transport_ms=600))
        s = valid_state(); s["link_age_ms"] = 2900
        self.assertIn("link_age", live.validate_state(s, transport_ms=101))

    def test_observed_reset_and_frozen_uptime_remain_failures(self):
        stats = live.CaptureStats()
        for uptime in (10000, 11000, 500, 500, 1500):
            s = valid_state(); s["uptime_ms"] = uptime
            stats.observe({"state": s, "reasons": live.validate_state(s)})
        self.assertEqual(stats.observed_resets, 1)
        self.assertEqual(stats.samples, 5)
        self.assertEqual(stats.failures, 2)
        self.assertEqual(stats.reasons["uptime_not_advancing"], 1)

    def test_lan_address_excludes_documentation_and_loopback(self):
        for value in ("192.168.1.50", "10.1.2.3", "172.16.1.8"):
            self.assertEqual(live.lan_address(value), value)
        for value in ("192.0.2.1", "127.0.0.1", "169.254.1.1", "8.8.8.8", "http://192.168.1.2"):
            with self.assertRaises(argparse.ArgumentTypeError): live.lan_address(value)

    def test_json_nonstandard_and_overflow_numbers_rejected(self):
        for raw in ('{"x":NaN}', '{"x":Infinity}', '{"x":1e999}'):
            with self.assertRaises(ValueError):
                json.loads(raw, parse_float=live.finite_float, parse_constant=live.invalid_constant)


if __name__ == "__main__":
    unittest.main()
