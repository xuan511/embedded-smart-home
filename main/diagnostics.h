// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
esp_err_t diagnostics_start(void);
bool diagnostics_submit(const char *name);
void diagnostics_message(char *buffer, size_t length);
