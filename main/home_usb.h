// SPDX-License-Identifier: MIT
#pragma once
#include "esp_err.h"
#include "home_link.h"
esp_err_t home_usb_start(void);
bool home_usb_poll(hl_message_t *message, bool *attached);
bool home_usb_send(const hl_message_t *message);
