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
    home_state_t state;
    uint64_t uptime_ms;
    uint32_t rejected_requests;
    bool simulated, led_ok, expander_ok, sd_ok;
} home_snapshot_t;

esp_err_t home_service_start(void);
bool home_submit(const home_request_t *request); // bounded, zero-wait queue
void home_snapshot(home_snapshot_t *snapshot);
esp_err_t home_console_start(void);
