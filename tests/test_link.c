// SPDX-License-Identifier: MIT
#include "home_link.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned feed(hl_parser_t *parser, const char *bytes, uint32_t now, hl_message_t *message)
{
    unsigned count = 0;
    while (*bytes) count += hl_parser_feed(parser, *bytes++, now, message);
    return count;
}

int main(void)
{
    hl_parser_t p; hl_parser_init(&p);
    hl_message_t m;
    assert(feed(&p, "H1,HEL", 0, &m) == 0);
    assert(feed(&p, "LO,17\r\nH1,PING,17,1\n", 20, &m) == 2);
    assert(m.type == HL_PING && m.v[0] == 17 && m.v[1] == 1);
    const char *bad[] = {
        "H1,HELLO,0\n", "H1,HELLO,-1\n", "H1,HELLO,+1\n", "H1,HELLO, 1\n",
        "H1,HELLO,1 \n", "H1,HELLO,2147483648\n", "H1,HELLO,-2147483649\n",
        "H1,HELLO,\n", "H1,HELLO,1,2\n", "H1,MODE,1,1,2\n", "H1,MODE,1,1,\n",
        "H1,SET,1,1,2,1\n", "H1,SET,1,1,1,2\n", "H1,NOPE,1\n", "H2,HELLO,1\n",
        "H1,PING,1,0\n", "H1,READY,1,5\n", "H1,STATUS,1,0,0,2,1,0,0,2500,5000,3000,8,0\n",
        "H1,STATUS,1,0,0,2,1,0,0,2500,5000,3000,7,-1\n", "H1,HELLO,1\r\r\n"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(*bad); ++i)
        assert(feed(&p, bad[i], 30, &m) == 0);
    assert(feed(&p, "H1,HELLO,2147483647\n", 40, &m) == 1 && m.v[0] == INT32_MAX);
    assert(feed(&p, "H1,CONFIG,1,1,-2147483648,1,0,1000,5000\n", 50, &m) == 1);
    assert(m.v[2] == INT32_MIN); // Syntax is valid; endpoint rejects configuration semantically.
    assert(feed(&p, "H1,HELLO,", 100, &m) == 0);
    hl_parser_expire(&p, 600);
    assert(feed(&p, "1\nH1,HELLO,2\n", 601, &m) == 1 && m.v[0] == 2);
    // Millisecond wrap does not disable the partial frame timeout.
    assert(feed(&p, "H1,HELLO,", UINT32_MAX - 100, &m) == 0);
    hl_parser_expire(&p, 400);
    assert(feed(&p, "2\n", 400, &m) == 0);
    for (unsigned i = 0; i < 400; ++i) assert(!hl_parser_feed(&p, 'x', 500, &m));
    assert(feed(&p, "\nH1,HELLO,3\n", 500, &m) == 1 && m.v[0] == 3);
    assert(!hl_parser_feed(&p, '\0', 600, &m));
    assert(feed(&p, "H1,HELLO,8\nH1,HELLO,4\n", 600, &m) == 1 && m.v[0] == 4);
    const hl_message_t messages[] = {
        {.type=HL_HELLO,.v={1}}, {.type=HL_READY,.v={1,4}},
        {.type=HL_CONFIG,.v={1,1,2800,2600,1000,2000,5000}},
        {.type=HL_MODE,.v={1,2,1}}, {.type=HL_SET,.v={1,3,0,1}}, {.type=HL_PING,.v={1,4}},
        {.type=HL_STATUS,.v={1,4,0,2,1,1,1,-4000,10000,1000000,7,INT32_MAX}}};
    for (unsigned i = 0; i < sizeof(messages) / sizeof(*messages); ++i) {
        char line[HL_FRAME_MAX + 1];
        size_t n = hl_encode(&messages[i], line, sizeof(line));
        assert(n && n <= HL_FRAME_MAX && line[n - 1] == '\n');
        assert(feed(&p, line, 1000, &m) == 1 && m.type == messages[i].type);
        assert(!memcmp(m.v, messages[i].v, sizeof(m.v)));
        assert(!hl_encode(&messages[i], line, n)); // Reserve NUL in caller buffer.
    }
    // Deterministic noisy-stream stress: bounded buffers and recovery after every LF.
    uint32_t random = 123;
    for (unsigned run = 0; run < 5000; ++run) {
        for (unsigned j = 0; j < run % 257; ++j) {
            random = random * 1664525u + 1013904223u;
            (void)hl_parser_feed(&p, (char)(random >> 24), 1100, &m);
        }
        (void)hl_parser_feed(&p, '\n', 1100, &m);
        assert(feed(&p, "H1,PING,1,1\n", 1100, &m) == 1);
    }
    puts("PASS: protocol fragmentation, coalescing, overflow, field validation, timeout/wrap, recovery, round trip");
    return 0;
}
