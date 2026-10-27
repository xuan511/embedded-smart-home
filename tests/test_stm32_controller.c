// SPDX-License-Identifier: MIT
#include "stm32_controller.h"
#include <assert.h>
#include <stdio.h>

static const hl_message_t hello = {.type=HL_HELLO,.v={17}};
static const hl_message_t config = {.type=HL_CONFIG,.v={17,1,2800,2600,1000,2000,5000}};
static home_sample_t sample(uint64_t at, float t, float lux)
{
    return (home_sample_t){.temperature_c=t,.humidity_pct=55,.illuminance_lux=lux,
        .temperature_valid=true,.humidity_valid=true,.light_valid=true,.timestamp_ms=at};
}
int main(void)
{
    stm32_controller_t c; hl_message_t out;
    sc_init(&c, 4);
    home_sample_t s = sample(0, 30, 50);
    sc_sample(&c, &s, 2); sc_tick(&c, 0);
    assert(!c.state.light_on && !c.state.fan_on); // Cold boot never arms before CONFIG.
    hl_message_t set = {.type=HL_SET,.v={17,2,0,1}};
    assert(!sc_receive(&c, &set, 0, &out)); // No handshake.
    assert(sc_receive(&c, &hello, 0, &out) && out.type == HL_READY && out.v[1] == 4);
    assert(sc_receive(&c, &set, 0, &out) && out.v[2] == 2); // Unsynced.
    hl_message_t cfg = config; cfg.v[1] = 3;
    assert(sc_receive(&c, &cfg, 1, &out) && out.v[2] == 0 && c.synced);
    assert(c.state.light_on && c.state.fan_on); // Automatic from local sample.
    hl_message_t bad = config; bad.v[1] = 4; bad.v[2] = bad.v[3];
    assert(sc_receive(&c, &bad, 2, &out) && out.v[2] == 1 && c.state.config.fan_on_c == 28);
    set.v[1] = 5; assert(sc_receive(&c, &set, 3, &out) && !c.state.automatic);
    set.v[3] = 0; // Same sequence cannot apply a different output command.
    assert(sc_receive(&c, &set, 4, &out) && c.state.light_on);
    set.v[1] = 4; assert(!sc_receive(&c, &set, 4, &out)); // Old command not replayed.
    sc_tick(&c, 5003); assert(c.state.light_on);
    sc_tick(&c, 5004); assert(!c.state.light_on && !c.state.fan_on);
    hl_message_t ping = {.type=HL_PING,.v={17,6}};
    assert(sc_receive(&c, &ping, 5100, &out) && !c.state.light_on); // Ping does not restore outputs.
    hl_message_t mode = {.type=HL_MODE,.v={17,7,1}};
    s = sample(6000, 30, 50); sc_sample(&c, &s, 2);
    assert(sc_receive(&c, &mode, 6000, &out) && c.state.automatic && c.state.fan_on);
    // New session requires CONFIG; existing automatic control continues valid local sensing.
    hl_message_t h2 = {.type=HL_HELLO,.v={18}};
    assert(sc_receive(&c, &h2, 7000, &out) && !c.synced);
    s = sample(12000, 29, 20); sc_sample(&c, &s, 2); sc_tick(&c, 12000);
    assert(c.state.fan_on && c.state.light_on); // Automatic survives host loss.
    set.v[0] = 17; set.v[1] = 100;
    assert(!sc_receive(&c, &set, 12001, &out)); // Wrong session.
    set.v[0] = 18;
    assert(sc_receive(&c, &set, 12001, &out) && out.v[2] == 2);
    s.temperature_valid = false; sc_sample(&c, &s, 2); sc_tick(&c, 12002);
    assert(!c.state.fan_on && c.state.light_on);
    sc_status(&c, 12002, 0, 0, &out); assert(out.v[10] == 6 && out.v[3] == 2);
    sc_tick(&c, 17000); assert(!c.state.light_on && c.state.light_fault);
    sc_status(&c, 17000, 0, 0, &out);
    assert(out.v[7] == 0 && out.v[8] == 0 && out.v[9] == 0 && out.v[10] == 0);
    // Duplicate HELLO in same session (retry) does not forget accepted config.
    sc_init(&c, 4); assert(sc_receive(&c, &hello, 0, &out));
    assert(sc_receive(&c, &config, 1, &out));
    assert(sc_receive(&c, &hello, 2, &out) && c.synced && c.last_seq == 1);
    // Return UART wire is broken: Korvo cannot receive STATUS and keeps starting
    // new sessions before 5 seconds. HELLO must not renew an old manual ON load.
    set = (hl_message_t){.type=HL_SET,.v={17,2,0,1}};
    assert(sc_receive(&c, &set, 10, &out) && c.state.light_on && !c.state.automatic);
    h2.v[0] = 18;
    assert(sc_receive(&c, &h2, 2000, &out) && !c.state.light_on && !c.state.fan_on && !c.synced);
    for (int32_t session = 19; session <= 22; ++session) {
        h2.v[0] = session;
        assert(sc_receive(&c, &h2, (uint64_t)session * 1000, &out));
        sc_tick(&c, (uint64_t)session * 1000);
        assert(!c.state.light_on && !c.state.fan_on && !c.synced);
    }
    for (unsigned stage = 1; stage <= 3; ++stage) {
        sc_init(&c, stage); assert(sc_receive(&c, &hello, 0, &out) && out.v[1] == (int32_t)stage);
        assert(sc_receive(&c, &config, 1, &out));
        set = (hl_message_t){.type=HL_SET,.v={17,2,0,1}};
        assert(sc_receive(&c, &set, 2, &out) && out.v[2] == (stage == 1 ? 3 : 0));
        set.v[1] = 3; set.v[2] = 1;
        assert(sc_receive(&c, &set, 3, &out) && out.v[2] == 3 && !c.state.fan_on);
        s = sample(4, 30, 50); sc_sample(&c, &s, stage == 2 ? 1 : 2);
        sc_status(&c, 4, 0, 0, &out);
        assert(out.v[3] == (stage == 1 ? 0 : stage == 2 ? 1 : 2));
    }
    puts("PASS: handshake/config order, ACK results, replay/session protection, manual timeout, autonomous rules, stages");
    return 0;
}
