// SPDX-License-Identifier: MIT
#pragma once
#include "esp_err.h"
#include "home_link.h"
esp_err_t home_usb_start(void);
// received_ms is the USB callback time, not the later queue-consumer time.
bool home_usb_poll(hl_message_t *message, bool *attached, uint32_t *received_ms);
bool home_usb_send(const hl_message_t *message);
