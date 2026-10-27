// SPDX-License-Identifier: MIT
#include "home_remote.h"
#include <limits.h>
#include <math.h>
#include <string.h>

static int32_t next_seq(home_remote_t *r)
{
    r->sequence = r->sequence == INT32_MAX ? 1 : r->sequence + 1;
    return r->sequence;
}

static home_config_t wire_config(const home_config_t *cfg)
{
    home_config_t c = home_config_valid(cfg) ? *cfg : home_default_config();
    c.fan_on_c = roundf(c.fan_on_c * 100) / 100;
    c.fan_off_c = roundf(c.fan_off_c * 100) / 100;
    c.light_on_lux = roundf(c.light_on_lux * 10) / 10;
    c.light_off_lux = roundf(c.light_off_lux * 10) / 10;
    return c;
}

void home_remote_init(home_remote_t *r, const home_config_t *cfg)
{
    memset(r, 0, sizeof(*r));
    home_config_t c = wire_config(cfg);
    home_init(&r->reported, &c);
    r->desired = r->reported.config;
    r->config_version = 1;
}

void home_remote_detach(home_remote_t *r)
{
    r->attached = r->ready = r->synced = r->pending = false;
    r->have_status = false;
    r->source = r->stage = 0;
}

void home_remote_attach(home_remote_t *r, int32_t session, uint32_t now)
{
    home_remote_detach(r);
    r->session = session > 0 ? session : 1;
    r->sequence = 0;
    r->attached = true;
    r->config_dirty = true;
    r->last_rx = now;
    r->last_hello = now - 1000u;
}

void home_remote_config(home_remote_t *r, const home_config_t *cfg)
{
    if (!home_config_valid(cfg)) return;
    home_config_t c = wire_config(cfg);
    if (!home_config_valid(&c)) return;
    r->desired = c;
    ++r->config_version;
    r->config_dirty = true;
    r->synced = false;
}

static void begin(home_remote_t *r, hl_type_t type, uint32_t now, hl_message_t *out)
{
    memset(out, 0, sizeof(*out));
    out->type = type;
    out->v[0] = r->session;
    out->v[1] = next_seq(r);
    r->pending = true;
    r->pending_seq = out->v[1];
    r->pending_type = type;
    r->sent_at = now;
}

bool home_remote_request(home_remote_t *r, hl_type_t type, int a, int b,
                         uint32_t now, hl_message_t *out)
{
    if (!r->attached || !r->synced || r->pending || r->config_dirty ||
        (uint32_t)(now - r->last_rx) >= 3000u || r->sequence == INT32_MAX ||
        (type != HL_MODE && type != HL_SET)) return false;
    begin(r, type, now, out);
    out->v[2] = a;
    out->v[3] = b;
    return true;
}

void home_remote_receive(home_remote_t *r, const hl_message_t *m, uint32_t now)
{
    home_remote_receive_at(r, m, now, now);
}

void home_remote_receive_at(home_remote_t *r, const hl_message_t *m,
                            uint32_t received_ms, uint32_t processed_ms)
{
    if (!r->attached || m->v[0] != r->session) return;
    // A stalled consumer must not make buffered old traffic fresh again.
    if ((uint32_t)(processed_ms - received_ms) >= 3000u) return;
    if (m->type == HL_READY) {
        // Repeated READY cannot cancel a CONFIG in flight.
        if (!r->ready) {
            r->ready = true;
            r->config_dirty = true;
        }
        r->stage = (int)m->v[1];
        r->last_rx = received_ms;
        return;
    }
    if (m->type != HL_STATUS || !r->ready) return;
    r->last_rx = received_ms;
    r->have_status = true;
    r->source = (int)m->v[3];
    r->reported.automatic = m->v[4] != 0;
    r->reported.light_on = m->v[5] != 0;
    r->reported.fan_on = m->v[6] != 0;
    home_sample_t p = {
        .temperature_c = m->v[7] / 100.0f,
        .humidity_pct = m->v[8] / 100.0f,
        .illuminance_lux = m->v[9] / 10.0f,
        .temperature_valid = (m->v[10] & 1) != 0,
        .humidity_valid = (m->v[10] & 2) != 0,
        .light_valid = (m->v[10] & 4) != 0,
        .timestamp_ms = received_ms
    };
    home_accept_sample(&r->reported, &p);
    r->sample_age = (uint32_t)m->v[11];
    r->sample_received = received_ms;
    // An ACK processed after the deadline cannot cancel the timeout/reconnect.
    if (r->pending && m->v[1] == r->pending_seq &&
        (uint32_t)(processed_ms - r->sent_at) < 1500u) {
        r->last_result = (int)m->v[2];
        r->timed_out = false;
        if (r->pending_type == HL_CONFIG && r->last_result == 0) {
            r->config_dirty = r->sent_version != r->config_version;
            r->synced = !r->config_dirty;
        } else if (r->pending_type == HL_CONFIG) {
            r->ready = r->synced = false;
            r->last_hello = processed_ms;
        }
        r->pending = false;
    }
}

bool home_remote_poll(home_remote_t *r, uint32_t now, hl_message_t *out)
{
    if (!r->attached) return false;
    if ((!r->pending && r->sequence == INT32_MAX) ||
        (uint32_t)(now - r->last_rx) >= 3000u ||
        (r->pending && (uint32_t)(now - r->sent_at) >= 1500u)) {
        // A new session makes late ACKs and old operations harmless.
        int32_t session = r->session == INT32_MAX ? 1 : r->session + 1;
        home_remote_attach(r, session, now);
        r->timed_out = true;
    }
    if (!r->ready) {
        if ((uint32_t)(now - r->last_hello) < 1000u) return false;
        *out = (hl_message_t){.type = HL_HELLO, .v = {r->session}};
        r->last_hello = now;
        return true;
    }
    if (r->pending) return false;
    if (r->config_dirty) {
        begin(r, HL_CONFIG, now, out);
        out->v[2] = (int32_t)lroundf(r->desired.fan_on_c * 100);
        out->v[3] = (int32_t)lroundf(r->desired.fan_off_c * 100);
        out->v[4] = (int32_t)lroundf(r->desired.light_on_lux * 10);
        out->v[5] = (int32_t)lroundf(r->desired.light_off_lux * 10);
        out->v[6] = (int32_t)r->desired.stale_ms;
        r->sent_version = r->config_version;
        return true;
    }
    if ((uint32_t)(now - r->last_ping) >= 1000u) {
        begin(r, HL_PING, now, out);
        r->last_ping = now;
        return true;
    }
    return false;
}

void home_remote_view(const home_remote_t *r, uint32_t now, home_state_t *out,
                      bool *online, bool *stale)
{
    *out = r->reported;
    out->config = r->desired;
    *online = r->attached && r->ready && r->have_status &&
        (uint32_t)(now - r->last_rx) < 3000u;
    uint64_t age = (uint64_t)r->sample_age + (uint32_t)(now - r->sample_received);
    *stale = !*online || !r->have_status || r->source == 0 || age >= r->desired.stale_ms;
    out->temperature_fault = *stale || !out->sample.temperature_valid;
    out->humidity_fault = *stale || !out->sample.humidity_valid;
    out->light_fault = *stale || !out->sample.light_valid;
    // Do not run home_tick here: only STM32 may calculate actual output states.
}
