// SPDX-License-Identifier: MIT
// HOST TEST FIXTURE ONLY: production protocol and controllers, test sensor values.
// No USB host, Wi-Fi, I2C or MOSFET hardware is exercised by this executable.
#include "home_gateway.h"
#include "home_remote.h"
#include "stm32_controller.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static home_remote_t remote;
static stm32_controller_t stm;
static uint32_t now = 100;
static bool attached = true;
static home_snapshot_t view;

static hl_message_t transfer(const hl_message_t *message)
{
    char frame[HL_FRAME_MAX + 1]; size_t n = hl_encode(message, frame, sizeof(frame));
    hl_parser_t parser; hl_parser_init(&parser); hl_message_t out = {0}; bool got = false;
    for (size_t i = 0; i < n; ++i) got |= hl_parser_feed(&parser, frame[i], now, &out);
    assert(got); return out;
}
static void exchange(const hl_message_t *message)
{
    hl_message_t request = transfer(message), reply;
    if (sc_receive(&stm, &request, now, &reply)) {
        hl_message_t answer = transfer(&reply); home_remote_receive(&remote, &answer, now);
    }
}
static void update(void)
{
    ++now; sc_tick(&stm, now);
    if (attached) {
        hl_message_t m;
        for (int i = 0; i < 4; ++i) if (home_remote_poll(&remote, now, &m)) exchange(&m);
        sc_status(&stm, now, 0, 0, &m); m = transfer(&m); home_remote_receive(&remote, &m, now);
    }
    home_remote_view(&remote, now, &view.state, &view.online, &view.stale);
    view.uptime_ms = now; view.synced = remote.synced; view.pending = remote.pending;
    view.source = remote.source; view.stage = remote.stage; view.outputs_known = view.online;
    view.sample_age_ms = remote.sample_age + (uint32_t)(now - remote.sample_received);
}
int main(void)
{
    home_config_t cfg = home_default_config(); home_remote_init(&remote, &cfg); sc_init(&stm, 4);
    home_remote_attach(&remote, 123, now);
    char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strncmp(line, "@sample ", 8)) {
            home_sample_t s = {.temperature_valid = true, .humidity_valid = true, .light_valid = true, .timestamp_ms = now};
            assert(sscanf(line + 8, "%f %f %f", &s.temperature_c, &s.humidity_pct, &s.illuminance_lux) == 3);
            sc_sample(&stm, &s, 1); // Always TEST source, even though this exercises stage-4 rules.
        } else if (!strncmp(line, "@fault", 6)) {
            home_sample_t s = {.timestamp_ms = now}; sc_sample(&stm, &s, 1);
        } else if (!strncmp(line, "@detach", 7)) {
            attached = false; home_remote_detach(&remote); now += 6000; sc_tick(&stm, now);
            assert(stm.state.automatic || (!stm.state.light_on && !stm.state.fan_on));
        } else if (!strncmp(line, "@attach", 7)) {
            attached = true; home_remote_attach(&remote, remote.session + 1, now);
        } else if (!strncmp(line, "@age", 4)) now += remote.desired.stale_ms + 1;
        else if (line[0] != '@') {
            home_request_t r;
            size_t n = strcspn(line, "\r\n");
            if (!home_api_decode(line, n, &remote.desired, &r)) {
                puts("{\"error\":\"invalid_command\"}"); fflush(stdout); continue;
            }
            update(); view.request_id = r.request_id;
            hl_message_t request;
            if (r.action == HOME_CONFIG) {
                home_remote_config(&remote, &r.config); view.request_result = HOME_RESULT_PENDING;
                // RAM-only fixture; do not claim a successful NVS save.
            } else {
                bool mode = r.action == HOME_AUTO || r.action == HOME_MANUAL_MODE;
                if (home_remote_request(&remote, mode ? HL_MODE : HL_SET,
                    mode ? r.action == HOME_AUTO : r.action == HOME_FAN_SET, r.on, now, &request)) {
                    exchange(&request); view.stm32_result = remote.last_result;
                    view.request_result = remote.last_result ? HOME_RESULT_REJECTED : HOME_RESULT_ACK;
                } else view.request_result = HOME_RESULT_REJECTED;
            }
        }
        update();
        if (view.request_result == HOME_RESULT_PENDING && remote.synced) view.request_result = HOME_RESULT_ACK;
        home_network_state_t net = {.enabled = true, .wifi = true};
        strcpy(net.ip, "127.0.0.1"); char json[HOME_API_STATE_MAX];
        assert(home_api_encode(&view, &net, json, sizeof(json))); puts(json); fflush(stdout);
    }
    return 0;
}
