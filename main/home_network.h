// SPDX-License-Identifier: MIT
#pragma once
#include "esp_err.h"
#include "home_model.h"
esp_err_t home_network_start(void);
void home_network_snapshot(home_network_state_t *state);
esp_err_t home_network_console_register(void);
