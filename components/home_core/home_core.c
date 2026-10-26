// SPDX-License-Identifier: MIT
#include "home_core.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

home_config_t home_default_config(void)
{
    return (home_config_t){28.0f, 26.0f, 100.0f, 200.0f, 5000};
}

bool home_config_valid(const home_config_t *c)
{
    return c && isfinite(c->fan_on_c) && isfinite(c->fan_off_c) &&
        isfinite(c->light_on_lux) && isfinite(c->light_off_lux) &&
        c->fan_off_c >= -40 && c->fan_on_c <= 125 && c->fan_off_c < c->fan_on_c &&
        c->light_on_lux >= 0 && c->light_on_lux < c->light_off_lux &&
        c->light_off_lux <= 100000 && c->stale_ms >= 1000 && c->stale_ms <= 60000;
}

void home_init(home_state_t *s, const home_config_t *c)
{
    memset(s, 0, sizeof(*s));
    s->config = home_config_valid(c) ? *c : home_default_config();
    s->automatic = true;
    s->temperature_fault = s->humidity_fault = s->light_fault = true;
}

void home_accept_sample(home_state_t *s, const home_sample_t *p)
{
    s->sample = *p;
    s->sample.temperature_valid &= isfinite(p->temperature_c) &&
        p->temperature_c >= -40 && p->temperature_c <= 125;
    s->sample.humidity_valid &= isfinite(p->humidity_pct) &&
        p->humidity_pct >= 0 && p->humidity_pct <= 100;
    s->sample.light_valid &= isfinite(p->illuminance_lux) &&
        p->illuminance_lux >= 0 && p->illuminance_lux <= 100000;
}

void home_tick(home_state_t *s, uint64_t now)
{
    const bool stale = now < s->sample.timestamp_ms ||
        now - s->sample.timestamp_ms >= s->config.stale_ms;
    s->temperature_fault = stale || !s->sample.temperature_valid;
    s->humidity_fault = stale || !s->sample.humidity_valid;
    s->light_fault = stale || !s->sample.light_valid;
    if (!s->automatic) return;
    // Invalid/stale channels turn off only their corresponding demonstration load.
    if (s->temperature_fault) s->fan_on = false;
    else if (s->sample.temperature_c >= s->config.fan_on_c) s->fan_on = true;
    else if (s->sample.temperature_c <= s->config.fan_off_c) s->fan_on = false;
    if (s->light_fault) s->light_on = false;
    else if (s->sample.illuminance_lux <= s->config.light_on_lux) s->light_on = true;
    else if (s->sample.illuminance_lux >= s->config.light_off_lux) s->light_on = false;
}

void home_manual(home_state_t *s, bool fan, bool on)
{
    s->automatic = false;
    if (fan) s->fan_on = on;
    else s->light_on = on;
}

uint8_t home_sht_crc(const uint8_t *data, unsigned length)
{
    uint8_t crc = 0xff;
    for (unsigned n = 0; n < length; ++n) {
        crc ^= data[n];
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

bool home_sht_decode(const uint8_t b[6], float *t, float *h)
{
    if (!b || !t || !h || home_sht_crc(b, 2) != b[2] || home_sht_crc(b + 3, 2) != b[5])
        return false;
    *t = -45.0f + 175.0f * ((unsigned)b[0] * 256 + b[1]) / 65535.0f;
    *h = 100.0f * ((unsigned)b[3] * 256 + b[4]) / 65535.0f;
    return true;
}
