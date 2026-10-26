// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float fan_on_c, fan_off_c;
    float light_on_lux, light_off_lux;
    uint32_t stale_ms;
} home_config_t;

typedef struct {
    float temperature_c, humidity_pct, illuminance_lux;
    bool temperature_valid, humidity_valid, light_valid;
    uint64_t timestamp_ms;
} home_sample_t;

typedef struct {
    home_config_t config;
    home_sample_t sample;
    bool automatic, light_on, fan_on;
    bool temperature_fault, humidity_fault, light_fault;
} home_state_t;

home_config_t home_default_config(void);
bool home_config_valid(const home_config_t *config);
void home_init(home_state_t *state, const home_config_t *config);
void home_accept_sample(home_state_t *state, const home_sample_t *sample);
void home_tick(home_state_t *state, uint64_t now_ms);
// Manual commands explicitly switch the entire controller to manual mode.
void home_manual(home_state_t *state, bool fan, bool on);
uint8_t home_sht_crc(const uint8_t *data, unsigned length);
bool home_sht_decode(const uint8_t bytes[6], float *temperature, float *humidity);
