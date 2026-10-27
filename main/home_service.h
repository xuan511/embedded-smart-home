// SPDX-License-Identifier: MIT
#pragma once
#include "esp_err.h"
#include "home_core.h"

typedef enum {
    HOME_AUTO, HOME_MANUAL_MODE, HOME_LIGHT_TOGGLE, HOME_FAN_TOGGLE,
    HOME_LIGHT_SET, HOME_FAN_SET, HOME_CONFIG, HOME_SIM, HOME_SIM_FAULT, HOME_SAVE
} home_action_t;

typedef struct {
    home_action_t action;
    bool on;
    home_config_t config;
    home_sample_t sample;
} home_request_t;

typedef struct {
    home_state_t state; // reported STM32 state, desired thresholds
    uint64_t uptime_ms;
    uint32_t rejected_requests;
    uint32_t sample_age_ms;
    uint32_t link_age_ms;
    bool simulated, sd_ok, online, synced, stale, pending, outputs_known;
    int source, stage;
    char request_status[96];
} home_snapshot_t;

esp_err_t home_service_start(void);
bool home_submit(const home_request_t *request);
void home_snapshot(home_snapshot_t *snapshot);
esp_err_t home_console_start(void);
