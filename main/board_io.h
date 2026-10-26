// SPDX-License-Identifier: MIT
#pragma once
#include "home_service.h"
void board_io_init(void);
void board_io_apply(const home_state_t *state);
void board_io_health(bool *led_ok, bool *expander_ok);
