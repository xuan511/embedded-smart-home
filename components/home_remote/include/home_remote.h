// SPDX-License-Identifier: MIT
#pragma once
#include "home_core.h"
#include "home_link.h"

// Korvo owns desired thresholds. Only STATUS updates the reported outputs.
typedef struct {
    home_state_t reported;
    home_config_t desired;
    int32_t session, sequence, pending_seq;
    hl_type_t pending_type;
    uint32_t last_rx, sent_at, last_hello, last_ping;
    uint32_t config_version, sent_version, sample_received;
    uint32_t sample_age;
    int source, stage, last_result;
    bool attached, ready, synced, config_dirty, have_status;
    bool pending, timed_out;
} home_remote_t;

void home_remote_init(home_remote_t *r, const home_config_t *cfg);
void home_remote_attach(home_remote_t *r, int32_t session, uint32_t now);
void home_remote_detach(home_remote_t *r);
void home_remote_config(home_remote_t *r, const home_config_t *cfg);
// One in-flight request. Manual operations are never replayed after timeout.
bool home_remote_request(home_remote_t *r, hl_type_t type, int a, int b,
                         uint32_t now, hl_message_t *out);
void home_remote_receive(home_remote_t *r, const hl_message_t *m, uint32_t now);
bool home_remote_poll(home_remote_t *r, uint32_t now, hl_message_t *out);
void home_remote_view(const home_remote_t *r, uint32_t now, home_state_t *out,
                      bool *online, bool *stale);
