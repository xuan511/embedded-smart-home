// SPDX-License-Identifier: MIT
#pragma once
#include "home_core.h"

typedef enum {
    HOME_AUTO, HOME_MANUAL_MODE, HOME_LIGHT_TOGGLE, HOME_FAN_TOGGLE,
    HOME_LIGHT_SET, HOME_FAN_SET, HOME_CONFIG, HOME_SIM, HOME_SIM_FAULT, HOME_SAVE
} home_action_t;

typedef enum {
    HOME_RESULT_NONE, HOME_RESULT_PENDING, HOME_RESULT_ACK, HOME_RESULT_REJECTED,
    HOME_RESULT_TIMEOUT, HOME_RESULT_SIMULATED, HOME_RESULT_SAVED, HOME_RESULT_SAVE_ERROR
} home_result_t;

typedef struct {
    home_action_t action;
    bool on, persist;
    uint32_t request_id; // 0 for LCD/console; phone supplies a positive correlation ID.
    home_config_t config;
    home_sample_t sample;
} home_request_t;

typedef struct {
    home_state_t state; // STM32 reports; desired thresholds belong to Korvo.
    uint64_t uptime_ms;
    uint32_t rejected_requests, sample_age_ms, link_age_ms;
    bool simulated, sd_ok, online, synced, stale, pending, outputs_known;
    int source, stage;
    uint32_t request_id, rejected_id;
    home_result_t request_result;
    int stm32_result;
    char request_status[96];
} home_snapshot_t;

typedef struct {
    bool enabled, wifi, mqtt, api;
    char ip[16];
} home_network_state_t;
