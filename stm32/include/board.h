// SPDX-License-Identifier: MIT
#pragma once
#include "home_core.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void board_init(void);
uint32_t board_millis(void);
void board_delay_ms(uint32_t ms);
bool board_uart_read(char *byte);
bool board_uart_lost_bytes(void);
void board_uart_write(const char *bytes, size_t length);
bool board_i2c_write(uint8_t address, const uint8_t *bytes, size_t length);
bool board_i2c_read(uint8_t address, uint8_t *bytes, size_t length);
void board_outputs(bool light_on, bool fan_on);
home_sample_t board_sensors_read(void);
