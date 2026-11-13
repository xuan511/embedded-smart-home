// SPDX-License-Identifier: MIT
#pragma once
#include "home_core.h"
#include "home_link.h"

#define SC_LINK_TIMEOUT_MS 5000u
typedef struct {
    home_state_t state;
    int32_t session, last_seq, last_result;
    uint64_t last_contact_ms;
    unsigned stage, source;
    bool synced, armed;
} stm32_controller_t;

void sc_init(stm32_controller_t *controller, unsigned stage);
void sc_tick(stm32_controller_t *controller, uint64_t now_ms);
void sc_sample(stm32_controller_t *controller, const home_sample_t *sample, unsigned source);
bool sc_receive(stm32_controller_t *controller, const hl_message_t *message,
                uint64_t now_ms, hl_message_t *reply);
void sc_status(stm32_controller_t *controller, uint64_t now_ms, int32_t seq,
               int32_t result, hl_message_t *reply);
