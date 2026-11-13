// SPDX-License-Identifier: MIT
// Both production state machines, using the production line codec on each hop.
#include "home_remote.h"
#include "stm32_controller.h"
#include <assert.h>
#include <stdio.h>

static hl_message_t on_wire(const hl_message_t *message, uint32_t now)
{
    char line[HL_FRAME_MAX + 1];
    const size_t length = hl_encode(message, line, sizeof(line));
    assert(length);
    hl_parser_t parser;
    hl_parser_init(&parser);
    hl_message_t decoded = {0};
    unsigned count = 0;
    for (size_t i = 0; i < length; ++i)
        count += hl_parser_feed(&parser, line[i], now + (uint32_t)(i / 8), &decoded);
    assert(count == 1);
    return decoded;
}

static void exchange(home_remote_t *remote, stm32_controller_t *stm, const hl_message_t *tx, uint32_t now)
{
    hl_message_t request = on_wire(tx, now), reply;
    assert(sc_receive(stm, &request, now, &reply));
    hl_message_t received = on_wire(&reply, now);
    home_remote_receive(remote, &received, now);
}

static home_sample_t sample(uint64_t now, float temp, float lux)
{
    return (home_sample_t){.temperature_c=temp,.humidity_pct=55,.illuminance_lux=lux,
        .temperature_valid=true,.humidity_valid=true,.light_valid=true,.timestamp_ms=now};
}

int main(void)
{
    home_remote_t remote;
    stm32_controller_t stm;
    hl_message_t tx;
    home_remote_init(&remote, NULL); sc_init(&stm, 4);
    home_remote_attach(&remote, 1234, 0);
    assert(home_remote_poll(&remote, 0, &tx) && tx.type == HL_HELLO);
    exchange(&remote, &stm, &tx, 0);
    assert(home_remote_poll(&remote, 10, &tx) && tx.type == HL_CONFIG);
    exchange(&remote, &stm, &tx, 10);
    assert(remote.synced && stm.synced);
    home_sample_t s = sample(100, 30, 50);
    sc_sample(&stm, &s, 2);
    assert(home_remote_poll(&remote, 1000, &tx) && tx.type == HL_PING);
    exchange(&remote, &stm, &tx, 1000);
    home_state_t view; bool online, stale;
    home_remote_view(&remote, 1000, &view, &online, &stale);
    assert(online && !stale && view.fan_on && view.light_on && view.sample.temperature_c == 30);
    assert(home_remote_request(&remote, HL_SET, 0, 0, 1100, &tx));
    assert(remote.reported.light_on); // Accepted request is not yet a device acknowledgement.
    exchange(&remote, &stm, &tx, 1100);
    assert(!remote.reported.light_on && !remote.reported.automatic && !stm.state.light_on);
    assert(home_remote_request(&remote, HL_SET, 1, 1, 1200, &tx));
    exchange(&remote, &stm, &tx, 1200);
    assert(stm.state.fan_on);
    // Bidirectional silence: device manual timer closes outputs; Korvo marks old data.
    sc_tick(&stm, 6200);
    assert(!stm.state.light_on && !stm.state.fan_on);
    home_remote_view(&remote, 6200, &view, &online, &stale);
    assert(!online && stale && view.fan_on); // Last reported output may differ; UI must label it as last known.
    home_config_t cfg = home_default_config(); cfg.fan_on_c = 31;
    home_remote_config(&remote, &cfg);
    assert(home_remote_poll(&remote, 6300, &tx) && tx.type == HL_HELLO);
    exchange(&remote, &stm, &tx, 6300);
    assert(home_remote_poll(&remote, 6310, &tx) && tx.type == HL_CONFIG);
    exchange(&remote, &stm, &tx, 6310);
    assert(remote.synced && stm.state.config.fan_on_c == 31 && !stm.state.fan_on && !stm.state.automatic);
    assert(home_remote_request(&remote, HL_MODE, 1, 0, 6400, &tx));
    s = sample(6400, 32, 20); sc_sample(&stm, &s, 2);
    exchange(&remote, &stm, &tx, 6400);
    assert(stm.state.automatic && stm.state.light_on && stm.state.fan_on);
    // Auto remains independent of Korvo while it receives fresh local samples.
    s = sample(13000, 32, 20); sc_sample(&stm, &s, 2); sc_tick(&stm, 13000);
    assert(stm.state.light_on && stm.state.fan_on);
    s.temperature_valid = false; sc_sample(&stm, &s, 2); sc_tick(&stm, 13001);
    assert(stm.state.light_on && !stm.state.fan_on);
    // STM32->Korvo wire lost while the request wire still works. Korvo cannot
    // see ACKs; its new-session HELLO must turn a previous manual ON command off.
    assert(!home_remote_request(&remote, HL_SET, 0, 1, 13010, &tx));
    assert(home_remote_poll(&remote, 13010, &tx) && tx.type == HL_HELLO);
    exchange(&remote, &stm, &tx, 13010);
    assert(home_remote_poll(&remote, 13020, &tx) && tx.type == HL_CONFIG);
    exchange(&remote, &stm, &tx, 13020);
    assert(home_remote_request(&remote, HL_SET, 0, 1, 13030, &tx));
    exchange(&remote, &stm, &tx, 13030);
    assert(stm.state.light_on && !stm.state.automatic);
    assert(home_remote_poll(&remote, 16100, &tx) && tx.type == HL_HELLO);
    hl_message_t request = on_wire(&tx, 16100), discarded_reply;
    assert(sc_receive(&stm, &request, 16100, &discarded_reply)); // Drop the reply on purpose.
    assert(!stm.state.light_on && !stm.state.fan_on);
    assert(home_remote_poll(&remote, 19100, &tx) && tx.type == HL_HELLO);
    request = on_wire(&tx, 19100);
    assert(sc_receive(&stm, &request, 19100, &discarded_reply));
    assert(!stm.state.light_on && !stm.state.fan_on);
    puts("PASS: real codec -> Korvo/STM32 handshake, readings, operation/ACK, timeout, reconnect configuration, auto faults");
    return 0;
}
