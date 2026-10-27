// SPDX-License-Identifier: MIT
#include "stm32_controller.h"
#include <limits.h>
#include <math.h>
#include <string.h>

void sc_init(stm32_controller_t *c, unsigned stage)
{
    memset(c, 0, sizeof(*c));
    c->stage = stage;
    home_init(&c->state, NULL);
}

void sc_sample(stm32_controller_t *c, const home_sample_t *sample, unsigned source)
{
    if (c->stage == 1) return;
    c->source = source;
    home_accept_sample(&c->state, sample);
}

void sc_tick(stm32_controller_t *c, uint64_t now)
{
    home_tick(&c->state, now);
    if (!c->state.automatic && now - c->last_contact_ms >= SC_LINK_TIMEOUT_MS)
        c->state.light_on = c->state.fan_on = false;
    if (!c->armed || c->stage == 1) c->state.light_on = c->state.fan_on = false;
    if (c->stage < 4) c->state.fan_on = false;
}

static int32_t scaled(float value, float scale, bool valid)
{
    return valid && isfinite(value) ? (int32_t)lroundf(value * scale) : 0;
}

void sc_status(stm32_controller_t *c, uint64_t now, int32_t seq, int32_t result, hl_message_t *out)
{
    sc_tick(c, now);
    const home_state_t *s = &c->state;
    uint64_t age = now >= s->sample.timestamp_ms ? now - s->sample.timestamp_ms : INT32_MAX;
    if (!c->source || age > INT32_MAX) age = INT32_MAX;
    *out = (hl_message_t){.type = HL_STATUS, .v = {
        c->session, seq, result, (int32_t)c->source, s->automatic, s->light_on, s->fan_on,
        scaled(s->sample.temperature_c, 100, !s->temperature_fault),
        scaled(s->sample.humidity_pct, 100, !s->humidity_fault),
        scaled(s->sample.illuminance_lux, 10, !s->light_fault),
        (!s->temperature_fault ? 1 : 0) | (!s->humidity_fault ? 2 : 0) | (!s->light_fault ? 4 : 0),
        (int32_t)age}};
}

bool sc_receive(stm32_controller_t *c, const hl_message_t *m, uint64_t now, hl_message_t *out)
{
    if (m->type == HL_HELLO && m->v[0] > 0) {
        if (m->v[0] != c->session) {
            c->session = m->v[0]; c->last_seq = 0; c->last_result = 0;
            c->synced = false;
            // A one-way broken return path can make Korvo retry new sessions.
            // Such HELLO traffic must never keep an old manual output energized.
            // Automatic control remains independent and keeps its valid local rules.
            if (!c->state.automatic) c->state.light_on = c->state.fan_on = false;
        }
        c->last_contact_ms = now;
        *out = (hl_message_t){.type = HL_READY, .v = {c->session, (int32_t)c->stage}};
        return true;
    }
    if (m->v[0] != c->session || !c->session || m->v[1] <= 0 ||
        (m->type != HL_CONFIG && m->type != HL_MODE && m->type != HL_SET && m->type != HL_PING))
        return false;
    // Stop-and-wait sender increments sequence numbers. Old commands never replay.
    if (m->v[1] < c->last_seq) return false;
    c->last_contact_ms = now;
    if (m->v[1] == c->last_seq) {
        sc_status(c, now, m->v[1], c->last_result, out);
        return true;
    }
    int32_t result = 0;
    if (m->type == HL_CONFIG) {
        home_config_t config = {m->v[2] / 100.0f, m->v[3] / 100.0f,
                                m->v[4] / 10.0f, m->v[5] / 10.0f, (uint32_t)m->v[6]};
        if (m->v[6] < 0 || !home_config_valid(&config)) result = 1;
        else {
            c->state.config = config; c->synced = true; c->armed = true;
        }
    } else if (m->type == HL_PING) {
        // Heartbeat works even before configuration; it does not arm outputs.
    } else if (!c->synced) result = 2;
    else if (c->stage == 1 || (m->type == HL_SET && m->v[2] == 1 && c->stage < 4)) result = 3;
    else if (m->type == HL_MODE) c->state.automatic = m->v[2] != 0;
    else if (m->type == HL_SET) home_manual(&c->state, m->v[2] != 0, m->v[3] != 0);
    c->last_seq = m->v[1]; c->last_result = result;
    sc_status(c, now, m->v[1], result, out);
    return true;
}
