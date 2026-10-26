// SPDX-License-Identifier: MIT
#include "sdkconfig.h"
#include "ui.h"
#if CONFIG_HOME_UI
#include <inttypes.h>
#include <stdint.h>
#include "bsp/esp-bsp.h"
#include "home_service.h"
#include "diagnostics.h"
#include "esp_log.h"

static lv_obj_t *summary, *readings, *outputs, *health, *diagnostic;

static void action(lv_event_t *event)
{
    home_request_t r = {.action = (home_action_t)(uintptr_t)lv_event_get_user_data(event)};
    if (!home_submit(&r)) ESP_LOGW("ui", "Command queue full");
}

static void test(lv_event_t *event)
{
    if (!diagnostics_submit(lv_event_get_user_data(event)))
        ESP_LOGW("ui", "Self-test queue full");
}

static lv_obj_t *label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_width(obj, LV_PCT(100));
    return obj;
}

static void button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *data)
{
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_set_size(obj, 165, 48);
    lv_obj_t *caption = lv_label_create(obj);
    lv_label_set_text(caption, text);
    lv_obj_center(caption);
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, data);
}

static lv_obj_t *row(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(obj, 4, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    return obj;
}

static void refresh(lv_timer_t *timer)
{
    (void)timer; // LVGL timer already holds the LVGL port lock.
    home_snapshot_t v;
    home_snapshot(&v);
    lv_label_set_text_fmt(summary, "EMBEDDED SMART HOME   |   %s   |   %s   |   %" PRIu64 " s",
        v.simulated ? "SIMULATED SENSORS" : "EXTERNAL SENSORS",
        v.state.automatic ? "AUTO" : "MANUAL", v.uptime_ms / 1000);
    lv_label_set_text_fmt(readings,
        "Temperature: %.1f C [%s]    Humidity: %.1f %% [%s]\nLight: %.1f lux [%s]",
        v.state.sample.temperature_c, v.state.temperature_fault ? "FAULT" : "OK",
        v.state.sample.humidity_pct, v.state.humidity_fault ? "FAULT" : "OK",
        v.state.sample.illuminance_lux, v.state.light_fault ? "FAULT" : "OK");
    lv_label_set_text_fmt(outputs,
        "Light command: %s     Fan command: %s\nFan ON/OFF %.1f/%.1f C; Light ON/OFF %.0f/%.0f lux",
        v.state.light_on ? "ON" : "OFF", v.state.fan_on ? "ON" : "OFF",
        v.state.config.fan_on_c, v.state.config.fan_off_c,
        v.state.config.light_on_lux, v.state.config.light_off_lux);
    lv_label_set_text_fmt(health, "RGB: %s   MCP output: %s   SD log: %s\nOutputs show commands; fan needs external hardware. Console: home help",
        v.led_ok ? "READY" : "ERROR", v.expander_ok ? "READY" : "UNAVAILABLE",
        v.sd_ok ? "READY" : "OFF/ERROR");
    char text[128];
    diagnostics_message(text, sizeof(text));
    lv_label_set_text(diagnostic, text);
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
    lv_obj_set_style_pad_row(screen, 10, 0);
    summary = label(screen, "Starting...");
    readings = label(screen, "Waiting for sensors...");
    outputs = label(screen, "Loads OFF");
    lv_obj_t *controls = row(screen);
    button(controls, "Light toggle", action, (void *)(uintptr_t)HOME_LIGHT_TOGGLE);
    button(controls, "Fan toggle", action, (void *)(uintptr_t)HOME_FAN_TOGGLE);
    button(controls, "Auto", action, (void *)(uintptr_t)HOME_AUTO);
    button(controls, "Save thresholds", action, (void *)(uintptr_t)HOME_SAVE);
    health = label(screen, "Board diagnostics");
    lv_obj_t *tests = row(screen);
    button(tests, "Speaker tone", test, "tone");
    button(tests, "Microphones", test, "mic");
    button(tests, "Memory", test, "memory");
    button(tests, "I2C scan", test, "i2c");
    diagnostic = label(screen, "Self-test idle");
    if (!lv_timer_create(refresh, 500, NULL)) {
        bsp_display_unlock();
        return ESP_ERR_NO_MEM;
    }
    bsp_display_unlock();
    return bsp_display_backlight_on();
#else
    return ESP_OK;
#endif
}
