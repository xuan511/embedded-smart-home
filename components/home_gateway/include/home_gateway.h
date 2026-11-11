// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include "home_model.h"
#define HOME_API_BODY_MAX 512
#define HOME_API_STATE_MAX 2048
// Strict JSON object. Unknown/duplicate keys, trailing data, NUL and non-finite
// numbers are rejected. No toggles: network commands always set absolute states.
bool home_api_decode(const char *body, size_t size, const home_config_t *current,
                     home_request_t *request);
bool home_api_encode(const home_snapshot_t *view, const home_network_state_t *net,
                     char *out, size_t capacity);
const char *home_result_name(home_result_t result);
