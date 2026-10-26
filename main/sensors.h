// SPDX-License-Identifier: MIT
#pragma once
#include "home_core.h"
void sensors_init(void);
home_sample_t sensors_read(uint64_t now_ms);
