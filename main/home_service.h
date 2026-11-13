// SPDX-License-Identifier: MIT
#pragma once
#include "esp_err.h"
#include "home_model.h"

esp_err_t home_service_start(void);
bool home_submit(const home_request_t *request);
void home_snapshot(home_snapshot_t *snapshot);
esp_err_t home_console_start(void);
