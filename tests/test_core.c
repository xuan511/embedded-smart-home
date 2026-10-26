// SPDX-License-Identifier: MIT
#include "home_core.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static home_sample_t sample(float t, float lux, uint64_t at)
{
    return (home_sample_t){.temperature_c = t, .humidity_pct = 50, .illuminance_lux = lux,
        .temperature_valid = true, .humidity_valid = true, .light_valid = true, .timestamp_ms = at};
}

int main(void)
{
    home_state_t s;
    home_init(&s, NULL);
    home_tick(&s, 0);
    assert(s.automatic && !s.fan_on && !s.light_on && s.temperature_fault && s.light_fault);
    home_sample_t p = sample(28, 100, 1000);
    home_accept_sample(&s, &p); home_tick(&s, 1000);
    assert(s.fan_on && s.light_on);
    for (int i = 0; i < 100; ++i) { // No chatter inside hysteresis bands.
        p = sample(26.1f + (i % 2) * 1.8f, 101 + (i % 2) * 98, 1100 + i);
        home_accept_sample(&s, &p); home_tick(&s, p.timestamp_ms);
        assert(s.fan_on && s.light_on);
    }
    p = sample(26, 200, 2000);
    home_accept_sample(&s, &p); home_tick(&s, 2000);
    assert(!s.fan_on && !s.light_on);
    p = sample(29, 50, 3000);
    home_accept_sample(&s, &p); home_tick(&s, 3000);
    p.temperature_valid = false;
    home_accept_sample(&s, &p); home_tick(&s, 3001);
    assert(!s.fan_on && s.light_on && s.temperature_fault && !s.light_fault);
    p.temperature_valid = true; p.light_valid = false;
    home_accept_sample(&s, &p); home_tick(&s, 3002);
    assert(s.fan_on && !s.light_on && !s.temperature_fault && s.light_fault);
    p = sample(30, 10, 4000);
    home_accept_sample(&s, &p); home_tick(&s, 8999);
    assert(s.fan_on && s.light_on);
    home_tick(&s, 9000); // Expire exactly at stale_ms, not one tick later.
    assert(!s.fan_on && !s.light_on && s.temperature_fault && s.humidity_fault && s.light_fault);
    home_manual(&s, true, true); home_tick(&s, 99999);
    assert(!s.automatic && s.fan_on); // Manual demonstration ignores automatic rules.
    s.automatic = true; home_tick(&s, 99999);
    assert(!s.fan_on);
    p = sample(NAN, INFINITY, 100000);
    home_accept_sample(&s, &p); home_tick(&s, 100000);
    assert(s.temperature_fault && s.light_fault);
    p = sample(200, -1, 100000);
    p.humidity_pct = 101;
    home_accept_sample(&s, &p); home_tick(&s, 100000);
    assert(!s.sample.temperature_valid && !s.sample.humidity_valid && !s.sample.light_valid);
    p = sample(30, 10, 100000);
    home_accept_sample(&s, &p); home_tick(&s, 99999); // Future timestamps are invalid.
    assert(s.temperature_fault && s.light_fault);
    home_config_t c = home_default_config();
    assert(home_config_valid(&c));
    c.fan_on_c = c.fan_off_c; assert(!home_config_valid(&c));
    c = home_default_config(); c.light_off_lux = NAN; assert(!home_config_valid(&c));
    c = home_default_config(); c.stale_ms = 0; assert(!home_config_valid(&c));
    home_init(&s, &c); assert(s.config.stale_ms == 5000); // Bad NVS falls back.
    const uint8_t known[] = {0xbe, 0xef};
    assert(home_sht_crc(known, 2) == 0x92); // Sensirion published CRC test vector.
    uint8_t raw[] = {0x66, 0x66, 0, 0x80, 0, 0};
    raw[2] = home_sht_crc(raw, 2); raw[5] = home_sht_crc(raw + 3, 2);
    float t, h;
    assert(home_sht_decode(raw, &t, &h));
    assert(fabsf(t - 25) < 0.01f && fabsf(h - 50) < 0.01f);
    raw[5] ^= 1; assert(!home_sht_decode(raw, &t, &h));
    raw[5] ^= 1; raw[2] ^= 1; assert(!home_sht_decode(raw, &t, &h));
    puts("PASS: startup, hysteresis, independent faults, stale data, modes, validation, SHT CRC/decoding");
    return 0;
}
