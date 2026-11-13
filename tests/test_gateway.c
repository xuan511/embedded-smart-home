// SPDX-License-Identifier: MIT
#include "home_gateway.h"
#include "cJSON.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static bool decode(const char *text, home_request_t *r)
{
    home_config_t c = home_default_config();
    return home_api_decode(text, strlen(text), &c, r);
}
int main(void)
{
    home_request_t r;
    assert(decode("{\"id\":1,\"action\":\"fan\",\"on\":true}", &r));
    assert(r.action == HOME_FAN_SET && r.on && r.request_id == 1);
    assert(decode("{\"id\":2147483647,\"action\":\"mode\",\"mode\":\"auto\"}", &r));
    assert(r.action == HOME_AUTO);
    assert(decode("{\"id\":2,\"action\":\"thresholds\",\"fan_on_c\":31.125,\"fan_off_c\":29,\"light_on_lux\":100,\"light_off_lux\":150}", &r));
    assert(r.action == HOME_CONFIG && r.persist && r.config.fan_on_c == 31.13f);
    const char *const bad[] = {
        "{}", "[]", "null", "{", "{\"id\":1,\"action\":\"fan\",\"on\":1}",
        "{\"id\":0,\"action\":\"fan\",\"on\":true}", "{\"id\":1.5,\"action\":\"fan\",\"on\":true}",
        "{\"id\":2147483648,\"action\":\"fan\",\"on\":true}",
        "{\"id\":1,\"action\":\"fan\",\"on\":true,\"extra\":0}",
        "{\"id\":1,\"action\":\"fan\",\"on\":{\"nested\":true}}",
        "{\"id\":1,\"id\":2,\"action\":\"fan\",\"on\":true}",
        "{\"id\":1,\"action\":\"fan\\u0000junk\",\"on\":true}",
        "{\"id\":1,\"action\":\"fan\",\"on\":true} garbage",
        "{\"id\":1,\"action\":\"toggle\",\"on\":true}",
        "{\"id\":1,\"action\":\"mode\",\"mode\":\"AUTO\"}",
        "{\"id\":1,\"action\":\"thresholds\",\"fan_on_c\":30.001,\"fan_off_c\":30,\"light_on_lux\":100,\"light_off_lux\":150}",
        "{\"id\":1,\"action\":\"thresholds\",\"fan_on_c\":1e309,\"fan_off_c\":29,\"light_on_lux\":100,\"light_off_lux\":150}"
    };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) assert(!decode(bad[i], &r));
    char oversized[HOME_API_BODY_MAX + 2]; memset(oversized, ' ', sizeof(oversized));
    home_config_t cfg = home_default_config();
    assert(!home_api_decode(oversized, sizeof(oversized), &cfg, &r));
    assert(!home_api_decode("{}\0bad", 6, &cfg, &r));
    home_snapshot_t view = {.state.config = cfg, .stale = true, .request_result = HOME_RESULT_TIMEOUT};
    home_network_state_t net = {0}; char out[HOME_API_STATE_MAX];
    assert(home_api_encode(&view, &net, out, sizeof(out)));
    cJSON *j = cJSON_Parse(out); assert(j);
    assert(cJSON_GetObjectItem(j, "stale_ms")->valuedouble == cfg.stale_ms);
    assert(cJSON_GetObjectItem(j, "link_age_ms")->valuedouble == view.link_age_ms);
    assert(cJSON_IsNull(cJSON_GetObjectItem(j, "temperature_c")));
    assert(cJSON_IsNull(cJSON_GetObjectItem(j, "fan_on")));
    assert(cJSON_IsFalse(cJSON_GetObjectItem(j, "physical_feedback")));
    cJSON_Delete(j);
    assert(!home_api_encode(&view, &net, out, 10));
    view.stale = false; view.outputs_known = true; view.source = 1;
    view.state.sample.temperature_c = 31; view.state.fan_on = true;
    assert(home_api_encode(&view, &net, out, sizeof(out)));
    j = cJSON_Parse(out); assert(j);
    assert(cJSON_GetObjectItem(j, "temperature_c")->valuedouble == 31);
    assert(!strcmp(cJSON_GetObjectItem(j, "source")->valuestring, "test"));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(j, "fan_on"))); cJSON_Delete(j);
    assert(!strcmp(home_network_hint(&net), "network_not_configured"));
    net.start_error = 123;
    assert(!strcmp(home_network_hint(&net), "network_start_failed"));
    net.start_error = 0; net.enabled = net.wifi_configured = true;
    net.last_wifi_reason = 201;
    assert(!strcmp(home_network_hint(&net), "wifi_disconnected"));
    net.wifi = true;
    assert(!strcmp(home_network_hint(&net), "mqtt_disabled"));
    net.mqtt_configured = true;
    assert(!strcmp(home_network_hint(&net), "mqtt_connecting"));
    net.mqtt_failure = HOME_MQTT_FAILURE_TRANSPORT;
    assert(!strcmp(home_network_hint(&net), "mqtt_transport_error"));
    net.mqtt_failure = HOME_MQTT_FAILURE_REFUSED;
    assert(!strcmp(home_network_hint(&net), "mqtt_broker_refused"));
    net.mqtt_failure = HOME_MQTT_FAILURE_AUTH;
    assert(!strcmp(home_network_hint(&net), "mqtt_auth_rejected"));
    net.mqtt = true;
    // Historical errors must not show an active failure after recovery.
    assert(!strcmp(home_network_hint(&net), "online"));
    net.last_mqtt_type = INT_MAX; net.last_mqtt_code = INT_MAX;
    net.last_mqtt_esp_error = INT_MIN; net.last_mqtt_socket_errno = INT_MAX;
    net.wifi_disconnect_events = net.mqtt_disconnect_events = UINT32_MAX;
    view.uptime_ms = UINT64_MAX; view.request_id = view.rejected_id = UINT32_MAX;
    view.link_age_ms = view.sample_age_ms = UINT32_MAX;
    memset(view.request_status, '"', sizeof(view.request_status) - 1);
    view.request_status[sizeof(view.request_status) - 1] = 0;
    strcpy(net.ip, "192.168.255.254");
    assert(home_api_encode(&view, &net, out, sizeof(out)));
    j = cJSON_Parse(out); assert(j);
    assert(!strcmp(cJSON_GetObjectItem(j, "network_hint")->valuestring, "online"));
    assert(cJSON_GetObjectItem(j, "last_wifi_reason")->valueint == 201);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(j, "mqtt_configured"))); cJSON_Delete(j);
    puts("PASS: phone JSON validation, thresholds, duplicate keys, overflow, stale/null outputs and no feedback claim");
    puts("PASS: network diagnostic precedence, disabled/connecting/error/recovery and maximum-size JSON");
    return 0;
}
