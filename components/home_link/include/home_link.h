// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Maximum wire length INCLUDING LF (and optional CR). No allocation.
#define HL_FRAME_MAX 192u
#define HL_FIELDS_MAX 12u
#define HL_PARTIAL_TIMEOUT_MS 500u
typedef enum { HL_HELLO, HL_READY, HL_CONFIG, HL_MODE, HL_SET, HL_PING, HL_STATUS } hl_type_t;
typedef struct { hl_type_t type; int32_t v[HL_FIELDS_MAX]; } hl_message_t;
typedef struct {
    char line[HL_FRAME_MAX];
    size_t length;
    uint32_t last_byte_ms;
    bool dropping;
    uint32_t rejected;
} hl_parser_t;

size_t hl_encode(const hl_message_t *message, char *out, size_t capacity);
void hl_parser_init(hl_parser_t *parser);
void hl_parser_expire(hl_parser_t *parser, uint32_t now_ms);
bool hl_parser_feed(hl_parser_t *parser, char byte, uint32_t now_ms, hl_message_t *out);
