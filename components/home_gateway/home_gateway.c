// SPDX-License-Identifier: MIT
#include "home_gateway.h"
#include "cJSON.h"
#include <limits.h>
#include <math.h>
#include <string.h>

const char *home_result_name(home_result_t result)
{
    static const char *const names[] = {"none", "pending", "ack", "rejected",
        "timeout", "simulated", "saved", "save_error"};
    return (unsigned)result < sizeof(names) / sizeof(names[0]) ? names[result] : "unknown";
}

const char *home_network_hint(const home_network_state_t *s)
{
    if (s->start_error) return "network_start_failed";
    if (!s->enabled || !s->wifi_configured) return "network_not_configured";
    if (!s->wifi) return "wifi_disconnected";
    if (!s->mqtt_configured) return "mqtt_disabled";
    if (s->mqtt) return "online";
    switch (s->mqtt_failure) {
    case HOME_MQTT_FAILURE_AUTH: return "mqtt_auth_rejected";
    case HOME_MQTT_FAILURE_REFUSED: return "mqtt_broker_refused";
    case HOME_MQTT_FAILURE_TRANSPORT: return "mqtt_transport_error";
    case HOME_MQTT_FAILURE_OTHER: return "mqtt_error";
    default: return "mqtt_connecting";
    }
}

static bool keys(const cJSON *object, const char *const *allowed, size_t count)
{
    size_t found = 0;
    for (const cJSON *p = object->child; p; p = p->next) {
        size_t i;
        for (i = 0; i < count; ++i) if (!strcmp(p->string, allowed[i])) break;
        if (i == count) return false;
        for (const cJSON *q = p->next; q; q = q->next)
            if (!strcmp(p->string, q->string)) return false;
        ++found;
    }
    return found == count;
}

static bool numeric(const cJSON *object, const char *key, float *value, float scale)
{
    const cJSON *p = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsNumber(p) || !isfinite(p->valuedouble) || fabs(p->valuedouble) > 100000) return false;
    *value = roundf((float)p->valuedouble * scale) / scale;
    return true;
}

bool home_api_decode(const char *body, size_t size, const home_config_t *current,
                     home_request_t *out)
{
    if (!body || !out || !current || !size || size > HOME_API_BODY_MAX ||
        memchr(body, 0, size) || memchr(body, '\\', size)) return false;
    // All supported commands are flat objects with fixed ASCII strings. Reject
    // nesting before cJSON recursion so a 512-byte malformed body cannot exhaust
    // the embedded HTTP task's stack.
    unsigned objects = 0;
    for (size_t i = 0; i < size; ++i)
        if (body[i] == '[' || (body[i] == '{' && ++objects > 1)) return false;
    char text[HOME_API_BODY_MAX + 1];
    memcpy(text, body, size); text[size] = 0;
    cJSON *json = cJSON_ParseWithLengthOpts(text, size + 1, NULL, true);
    if (!cJSON_IsObject(json)) { cJSON_Delete(json); return false; }
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
    const cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    home_request_t r = {0};
    bool valid = cJSON_IsNumber(id) && isfinite(id->valuedouble) &&
        id->valuedouble >= 1 && id->valuedouble <= INT32_MAX &&
        id->valuedouble == floor(id->valuedouble) && cJSON_IsString(action);
    if (valid) {
        r.request_id = (uint32_t)id->valuedouble;
        if (!strcmp(action->valuestring, "mode")) {
            const char *const allowed[] = {"id", "action", "mode"};
            const cJSON *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
            valid = keys(json, allowed, 3) && cJSON_IsString(mode) &&
                (!strcmp(mode->valuestring, "auto") || !strcmp(mode->valuestring, "manual"));
            if (valid) r.action = !strcmp(mode->valuestring, "auto") ? HOME_AUTO : HOME_MANUAL_MODE;
        } else if (!strcmp(action->valuestring, "fan") || !strcmp(action->valuestring, "light")) {
            const char *const allowed[] = {"id", "action", "on"};
            const cJSON *on = cJSON_GetObjectItemCaseSensitive(json, "on");
            valid = keys(json, allowed, 3) && cJSON_IsBool(on);
            r.action = !strcmp(action->valuestring, "fan") ? HOME_FAN_SET : HOME_LIGHT_SET;
            r.on = cJSON_IsTrue(on);
        } else if (!strcmp(action->valuestring, "thresholds")) {
            const char *const allowed[] = {"id", "action", "fan_on_c", "fan_off_c", "light_on_lux", "light_off_lux"};
            r.action = HOME_CONFIG; r.persist = true; r.config = *current;
            valid = keys(json, allowed, 6) &&
                numeric(json, "fan_on_c", &r.config.fan_on_c, 100) &&
                numeric(json, "fan_off_c", &r.config.fan_off_c, 100) &&
                numeric(json, "light_on_lux", &r.config.light_on_lux, 10) &&
                numeric(json, "light_off_lux", &r.config.light_off_lux, 10) &&
                home_config_valid(&r.config);
        } else valid = false;
    }
    cJSON_Delete(json);
    if (valid) *out = r;
    return valid;
}

bool home_api_encode(const home_snapshot_t *v, const home_network_state_t *net,
                     char *out, size_t capacity)
{
    if (!v || !net || !out || capacity > INT_MAX) return false;
    cJSON *j = cJSON_CreateObject();
    if (!j) return false;
    bool ok = true;
#define BOOL(k, x) do { if (!cJSON_AddBoolToObject(j, k, (x))) ok = false; } while (0)
#define NUM(k, x) do { if (!cJSON_AddNumberToObject(j, k, (double)(x))) ok = false; } while (0)
#define STR(k, x) do { if (!cJSON_AddStringToObject(j, k, (x))) ok = false; } while (0)
#define READING(k, x, fault) do { if (fault) { if (!cJSON_AddNullToObject(j, k)) ok = false; } else NUM(k, x); } while (0)
    NUM("schema", 1); NUM("uptime_ms", v->uptime_ms); NUM("stage", v->stage);
    STR("source", v->simulated ? "simulation" : v->source == 1 ? "test" : v->source == 2 ? "sensor" : "none");
    BOOL("simulated", v->simulated); BOOL("online", v->online); BOOL("synced", v->synced);
    BOOL("stale", v->stale); NUM("sample_age_ms", v->sample_age_ms);
    NUM("stale_ms", v->state.config.stale_ms);
    NUM("link_age_ms", v->link_age_ms);
    READING("temperature_c", v->state.sample.temperature_c, v->stale || v->state.temperature_fault);
    READING("humidity_pct", v->state.sample.humidity_pct, v->stale || v->state.humidity_fault);
    READING("illuminance_lux", v->state.sample.illuminance_lux, v->stale || v->state.light_fault);
    BOOL("temperature_fault", v->state.temperature_fault); BOOL("humidity_fault", v->state.humidity_fault);
    BOOL("light_fault", v->state.light_fault); BOOL("outputs_known", v->outputs_known || v->simulated);
    if (v->outputs_known || v->simulated) {
        STR("mode", v->state.automatic ? "auto" : "manual");
        BOOL("light_on", v->state.light_on); BOOL("fan_on", v->state.fan_on);
    } else {
        STR("mode", "unknown");
        if (!cJSON_AddNullToObject(j, "light_on")) ok = false;
        if (!cJSON_AddNullToObject(j, "fan_on")) ok = false;
    }
    BOOL("physical_feedback", false); BOOL("pending", v->pending); BOOL("sd_ok", v->sd_ok);
    NUM("fan_on_c", v->state.config.fan_on_c); NUM("fan_off_c", v->state.config.fan_off_c);
    NUM("light_on_lux", v->state.config.light_on_lux); NUM("light_off_lux", v->state.config.light_off_lux);
    NUM("request_id", v->request_id); STR("request_result", home_result_name(v->request_result));
    NUM("rejected_id", v->rejected_id);
    NUM("stm32_result", v->stm32_result); STR("request_status", v->request_status);
    BOOL("wifi", net->wifi); BOOL("mqtt", net->mqtt); STR("ip", net->ip);
    BOOL("wifi_configured", net->wifi_configured); BOOL("mqtt_configured", net->mqtt_configured);
    STR("network_hint", home_network_hint(net)); NUM("network_error", net->start_error);
    NUM("last_wifi_reason", net->last_wifi_reason);
    NUM("last_mqtt_type", net->last_mqtt_type); NUM("last_mqtt_code", net->last_mqtt_code);
    NUM("last_mqtt_esp_error", net->last_mqtt_esp_error); NUM("last_mqtt_errno", net->last_mqtt_socket_errno);
    NUM("wifi_disconnect_events", net->wifi_disconnect_events); NUM("mqtt_disconnect_events", net->mqtt_disconnect_events);
    ok = ok && cJSON_PrintPreallocated(j, out, (int)capacity, false);
    cJSON_Delete(j);
    return ok;
#undef BOOL
#undef NUM
#undef STR
#undef READING
}
