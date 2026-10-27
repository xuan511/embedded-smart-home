// SPDX-License-Identifier: MIT
#include "home_link.h"
#include <limits.h>
#include <string.h>

static const char *const names[] = {"HELLO", "READY", "CONFIG", "MODE", "SET", "PING", "STATUS"};
static const unsigned counts[] = {1, 2, 7, 3, 4, 2, 12};

static bool valid(const hl_message_t *m)
{
    if (!m || (unsigned)m->type > HL_STATUS || m->v[0] <= 0) return false;
    const int32_t *v = m->v;
    switch (m->type) {
    case HL_HELLO: return true;
    case HL_READY: return v[1] >= 1 && v[1] <= 4;
    case HL_CONFIG: return v[1] > 0; // Controller returns BAD_CONFIG for invalid thresholds.
    case HL_MODE: return v[1] > 0 && (v[2] == 0 || v[2] == 1);
    case HL_SET: return v[1] > 0 && v[2] >= 0 && v[2] <= 1 && v[3] >= 0 && v[3] <= 1;
    case HL_PING: return v[1] > 0;
    case HL_STATUS:
        return v[1] >= 0 && v[2] >= 0 && v[2] <= 3 && v[3] >= 0 && v[3] <= 2 &&
            v[4] >= 0 && v[4] <= 1 && v[5] >= 0 && v[5] <= 1 && v[6] >= 0 && v[6] <= 1 &&
            v[7] >= -4000 && v[7] <= 12500 && v[8] >= 0 && v[8] <= 10000 &&
            v[9] >= 0 && v[9] <= 1000000 && v[10] >= 0 && v[10] <= 7 && v[11] >= 0;
    }
    return false;
}

static size_t integer(char *out, int32_t value)
{
    char reverse[10];
    size_t n = 0, used = 0;
    uint32_t magnitude = value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    if (value < 0) out[used++] = '-';
    do { reverse[n++] = (char)('0' + magnitude % 10); magnitude /= 10; } while (magnitude);
    while (n) out[used++] = reverse[--n];
    return used;
}

size_t hl_encode(const hl_message_t *m, char *out, size_t capacity)
{
    if (!valid(m) || !out) return 0;
    char line[HL_FRAME_MAX];
    size_t used = 0;
    memcpy(line, "H1,", 3); used = 3;
    const size_t name_length = strlen(names[m->type]);
    memcpy(line + used, names[m->type], name_length); used += name_length;
    for (unsigned i = 0; i < counts[m->type]; ++i) {
        line[used++] = ',';
        used += integer(line + used, m->v[i]);
    }
    line[used++] = '\n';
    if (used > HL_FRAME_MAX || capacity < used + 1) return 0;
    memcpy(out, line, used);
    out[used] = '\0';
    return used;
}

static bool decimal(const char *first, const char *end, int32_t *value)
{
    bool negative = false;
    if (first < end && *first == '-') { negative = true; ++first; }
    if (first == end) return false;
    uint32_t n = 0;
    const uint32_t limit = negative ? 2147483648u : 2147483647u;
    for (; first < end; ++first) {
        if (*first < '0' || *first > '9') return false;
        const unsigned digit = (unsigned)(*first - '0');
        if (n > (limit - digit) / 10u) return false;
        n = n * 10u + digit;
    }
    *value = negative ? (int32_t)(-(int64_t)n) : (int32_t)n;
    return true;
}

static bool decode(char *line, size_t length, hl_message_t *out)
{
    if (length && line[length - 1] == '\r') --length;
    if (length < 5 || memcmp(line, "H1,", 3)) return false;
    size_t p = 3, end = p;
    while (end < length && line[end] != ',') ++end;
    hl_message_t m = {0};
    bool found = false;
    for (unsigned i = 0; i <= HL_STATUS; ++i)
        if (strlen(names[i]) == end - p && !memcmp(line + p, names[i], end - p)) {
            m.type = (hl_type_t)i; found = true; break;
        }
    if (!found) return false;
    p = end;
    for (unsigned i = 0; i < counts[m.type]; ++i) {
        if (p >= length || line[p++] != ',') return false;
        end = p;
        while (end < length && line[end] != ',') ++end;
        if (!decimal(line + p, line + end, &m.v[i])) return false;
        p = end;
    }
    if (p != length || !valid(&m)) return false;
    *out = m;
    return true;
}

void hl_parser_init(hl_parser_t *p) { memset(p, 0, sizeof(*p)); }
void hl_parser_expire(hl_parser_t *p, uint32_t now)
{
    if (p->length && (uint32_t)(now - p->last_byte_ms) >= HL_PARTIAL_TIMEOUT_MS) {
        p->length = 0; p->dropping = true; ++p->rejected;
    }
}
bool hl_parser_feed(hl_parser_t *p, char byte, uint32_t now, hl_message_t *out)
{
    if (!p || !out) return false;
    hl_parser_expire(p, now);
    p->last_byte_ms = now;
    if (byte == '\n') {
        bool ok = false;
        if (!p->dropping) {
            ok = decode(p->line, p->length, out);
            if (!ok) ++p->rejected;
        }
        p->dropping = false; p->length = 0;
        return ok;
    }
    if (p->dropping) return false;
    if (p->length >= HL_FRAME_MAX - 1 || byte == '\0' || (unsigned char)byte > 127) {
        p->length = 0; p->dropping = true; ++p->rejected;
        return false;
    }
    p->line[p->length++] = byte;
    return false;
}
