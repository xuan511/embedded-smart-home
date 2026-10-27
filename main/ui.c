// SPDX-License-Identifier: MIT
#include "sdkconfig.h"
#include "ui.h"
#if CONFIG_HOME_UI
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "home_service.h"
#include "esp_log.h"

static lv_obj_t *summary, *readings, *outputs, *health, *request;
static void action(lv_event_t *event)
{
    home_request_t r = {.action = (home_action_t)(uintptr_t)lv_event_get_user_data(event)};
    if (!home_submit(&r)) ESP_LOGW("ui", "Request queue full");
}
static lv_obj_t *label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_width(obj, LV_PCT(100));
    return obj;
}
static void button(lv_obj_t *parent, const char *text, home_action_t action_id)
{
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_set_size(obj, 170, 52);
    lv_obj_t *caption = lv_label_create(obj);
    lv_label_set_text(caption, text);
    lv_obj_center(caption);
    lv_obj_add_event_cb(obj, action, LV_EVENT_CLICKED, (void *)(uintptr_t)action_id);
}
static void refresh(lv_timer_t *timer)
{
    (void)timer;
    home_snapshot_t v;
    home_snapshot(&v);
    const char *source = v.simulated ? "OFFLINE SIMULATION" :
        (v.source == 1 ? "STM32 TEST DATA" : (v.source == 2 ? "STM32 SENSORS" : "WAITING / NO DATA"));
    lv_label_set_text_fmt(summary, "SMART HOME | %s | stage %d | %" PRIu64 " s",
        source, v.stage, v.uptime_ms / 1000);
    char t[32] = "--", h[32] = "--", l[32] = "--";
    if (!v.state.temperature_fault) snprintf(t, sizeof(t), "%.1f C", v.state.sample.temperature_c);
    if (!v.state.humidity_fault) snprintf(h, sizeof(h), "%.1f %%", v.state.sample.humidity_pct);
    if (!v.state.light_fault) snprintf(l, sizeof(l), "%.1f lux", v.state.sample.illuminance_lux);
    lv_label_set_text_fmt(readings, "Temperature: %s [%s]   Humidity: %s [%s]\nLight: %s [%s]",
        t, v.state.temperature_fault ? (v.stale ? "STALE" : "FAULT") : "OK",
        h, v.state.humidity_fault ? (v.stale ? "STALE" : "FAULT") : "OK",
        l, v.state.light_fault ? (v.stale ? "STALE" : "FAULT") : "OK");
    bool known = v.outputs_known || v.simulated;
    lv_label_set_text_fmt(outputs, "%s mode: %s | Light: %s | Fan: %s\nDesired thresholds: Fan %.2f/%.2f C; Light %.1f/%.1f lux",
        v.simulated ? "Simulated" : "STM32 reported",
        known ? (v.state.automatic ? "AUTO" : "MANUAL") : "UNKNOWN",
        known ? (v.state.light_on ? "ON" : "OFF") : "UNKNOWN",
        known ? (v.state.fan_on ? "ON" : "OFF") : "UNKNOWN",
        v.state.config.fan_on_c, v.state.config.fan_off_c,
        v.state.config.light_on_lux, v.state.config.light_off_lux);
    lv_label_set_text_fmt(health, "Link: %s | Config: %s | SD: %s\nNo physical load feedback. Thresholds: serial home help",
        v.simulated ? "SIMULATION" : (v.online ? "ONLINE" : "DISCONNECTED"),
        v.synced ? "SYNCED" : "SYNCING", v.sd_ok ? "READY" : "OFF/ERROR");
    lv_label_set_text(request, v.request_status);
}
#endif
esp_err_t home_ui_start(void)
{
#if CONFIG_HOME_UI
    if (!bsp_display_start()) return ESP_FAIL;
    if (!bsp_display_lock(3000)) return ESP_ERR_TIMEOUT;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101c2e), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xe5f0fa), 0);
    lv_obj_set_style_pad_all(screen, 16, 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(screen, 14, 0);
    summary = label(screen, "Starting...");
    readings = label(screen, "Waiting for STM32...");
    outputs = label(screen, "Output status UNKNOWN");
    lv_obj_t *controls = lv_obj_create(screen);
    lv_obj_set_size(controls, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(controls, 4, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    button(controls, "Light toggle", HOME_LIGHT_TOGGLE);
    button(controls, "Fan toggle", HOME_FAN_TOGGLE);
    button(controls, "Manual", HOME_MANUAL_MODE);
    button(controls, "Auto", HOME_AUTO);
    health = label(screen, "Waiting for link");
    request = label(screen, "No operation requested");
    if (!lv_timer_create(refresh, 250, NULL)) {
        bsp_display_unlock();
        return ESP_ERR_NO_MEM;
    }
    bsp_display_unlock();
    return bsp_display_backlight_on();
#else
    return ESP_OK;
#endif
}
