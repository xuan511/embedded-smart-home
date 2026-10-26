// SPDX-License-Identifier: MIT
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "home_service.h"
#include "board_io.h"
#include "sensors.h"
#include "diagnostics.h"
#include "bsp/esp-bsp.h"
#include "esp_console.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#if CONFIG_HOME_REAL_SENSORS
#define HOME_IS_SIMULATED false
#else
#define HOME_IS_SIMULATED true
#endif

static const char *TAG = "home";
static QueueHandle_t requests;
static SemaphoreHandle_t snapshot_lock;
static home_snapshot_t published;
static uint32_t rejected;
static portMUX_TYPE count_lock = portMUX_INITIALIZER_UNLOCKED;

uint64_t home_now_ms(void) { return (uint64_t)esp_timer_get_time() / 1000; }

bool home_submit(const home_request_t *r)
{
    if (requests && r && xQueueSend(requests, r, 0) == pdTRUE) return true;
    portENTER_CRITICAL(&count_lock);
    ++rejected;
    portEXIT_CRITICAL(&count_lock);
    return false;
}

void home_snapshot(home_snapshot_t *s)
{
    xSemaphoreTake(snapshot_lock, portMAX_DELAY);
    *s = published;
    xSemaphoreGive(snapshot_lock);
}

static void load_config(home_config_t *c)
{
    nvs_handle_t h;
    if (nvs_open("home", NVS_READONLY, &h) != ESP_OK) return;
    size_t size = sizeof(*c);
    home_config_t saved;
    if (nvs_get_blob(h, "config_v1", &saved, &size) == ESP_OK &&
        size == sizeof(saved) && home_config_valid(&saved)) *c = saved;
    nvs_close(h);
}

static esp_err_t save_config(const home_config_t *c)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("home", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, "config_v1", c, sizeof(*c));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static bool log_csv(const home_state_t *s, uint64_t now)
{
    FILE *f = fopen(BSP_SD_MOUNT_POINT "/home.csv", "a");
    if (!f) return false;
    bool ok = true;
    if (ftell(f) == 0)
        ok = fprintf(f, "uptime_ms,simulated,auto,temp_c,rh_pct,lux,temp_valid,rh_valid,light_valid,light_command,fan_command\n") > 0;
    ok &= fprintf(f, "%" PRIu64 ",%d,%d,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d\n",
        now, HOME_IS_SIMULATED, s->automatic,
        s->sample.temperature_c, s->sample.humidity_pct, s->sample.illuminance_lux,
        !s->temperature_fault, !s->humidity_fault, !s->light_fault,
        s->light_on, s->fan_on) > 0;
    if (fclose(f) != 0) ok = false;
    return ok;
}

static void controller_task(void *arg)
{
    (void)arg;
    home_config_t cfg = home_default_config();
    load_config(&cfg);
    home_state_t s;
    home_init(&s, &cfg);
#if !CONFIG_HOME_REAL_SENSORS
    home_sample_t sim = {.temperature_c = 25, .humidity_pct = 50, .illuminance_lux = 300,
        .temperature_valid = true, .humidity_valid = true, .light_valid = true};
#endif
    uint64_t next_sample = 0, next_log = 10000, next_status = 0;
    bool sd_ok = false;
#if CONFIG_HOME_SD_LOG
    sd_ok = bsp_sdcard_mount() == ESP_OK;
    ESP_LOGI(TAG, "SD logging: %s", sd_ok ? "ready" : "unavailable; continuing");
#endif
    for (;;) {
        home_request_t r;
        if (xQueueReceive(requests, &r, pdMS_TO_TICKS(50)) == pdTRUE) {
            switch (r.action) {
            case HOME_AUTO: s.automatic = true; break;
            case HOME_MANUAL_MODE: s.automatic = false; break;
            case HOME_LIGHT_TOGGLE: home_manual(&s, false, !s.light_on); break;
            case HOME_FAN_TOGGLE: home_manual(&s, true, !s.fan_on); break;
            case HOME_LIGHT_SET: home_manual(&s, false, r.on); break;
            case HOME_FAN_SET: home_manual(&s, true, r.on); break;
            case HOME_CONFIG:
                if (home_config_valid(&r.config)) s.config = r.config;
                else ESP_LOGW(TAG, "Invalid thresholds rejected");
                break;
            case HOME_SIM:
#if !CONFIG_HOME_REAL_SENSORS
                sim = r.sample;
                next_sample = 0;
#endif
                break;
            case HOME_SIM_FAULT:
#if !CONFIG_HOME_REAL_SENSORS
                sim.temperature_valid = sim.humidity_valid = sim.light_valid = false;
                next_sample = 0;
#endif
                break;
            case HOME_SAVE:
                ESP_LOGI(TAG, "Save thresholds: %s", esp_err_to_name(save_config(&s.config)));
                break;
            }
        }
        uint64_t now = home_now_ms();
        if (now >= next_sample) {
#if CONFIG_HOME_REAL_SENSORS
            home_sample_t p = sensors_read(now);
#else
            home_sample_t p = sim;
            p.timestamp_ms = now;
#endif
            home_accept_sample(&s, &p);
            next_sample = now + 1000;
        }
        now = home_now_ms();
        home_tick(&s, now);
        board_io_apply(&s);
        if (sd_ok && now >= next_log) {
            sd_ok = log_csv(&s, now);
            next_log = now + 10000;
            if (!sd_ok) ESP_LOGW(TAG, "SD write failed; logging stopped until reboot");
        }
        home_snapshot_t view = {.state = s, .uptime_ms = now, .sd_ok = sd_ok,
            .simulated = HOME_IS_SIMULATED};
        board_io_health(&view.led_ok, &view.expander_ok);
        portENTER_CRITICAL(&count_lock);
        view.rejected_requests = rejected;
        portEXIT_CRITICAL(&count_lock);
        xSemaphoreTake(snapshot_lock, portMAX_DELAY);
        published = view;
        xSemaphoreGive(snapshot_lock);
        if (now >= next_status) {
            ESP_LOGI(TAG, "%s %s T=%.1f RH=%.1f L=%.1f | light=%d fan=%d faults=%d/%d",
                view.simulated ? "SIMULATED" : "REAL", s.automatic ? "AUTO" : "MANUAL",
                s.sample.temperature_c, s.sample.humidity_pct, s.sample.illuminance_lux,
                s.light_on, s.fan_on, s.temperature_fault, s.light_fault);
            next_status = now + 10000;
        }
    }
}

esp_err_t home_service_start(void)
{
    snapshot_lock = xSemaphoreCreateMutex();
    requests = xQueueCreate(16, sizeof(home_request_t));
    if (!snapshot_lock || !requests) return ESP_ERR_NO_MEM;
    home_init(&published.state, NULL);
    published.simulated = HOME_IS_SIMULATED;
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "BSP I2C failed");
    board_io_init();
    sensors_init();
    return xTaskCreate(controller_task, "home_control", 6144, NULL, 5, NULL) == pdPASS
        ? ESP_OK : ESP_ERR_NO_MEM;
}

static bool number(const char *text, float *value)
{
    char *end;
    *value = strtof(text, &end);
    return text[0] != '\0' && *end == '\0' && isfinite(*value);
}

static int command(int argc, char **argv)
{
    home_snapshot_t v;
    home_snapshot(&v);
    home_request_t r = {0};
    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        printf("%s | %s | uptime=%" PRIu64 "ms | rejected=%" PRIu32 "\n",
            v.simulated ? "SIMULATED" : "REAL", v.state.automatic ? "AUTO" : "MANUAL",
            v.uptime_ms, v.rejected_requests);
        printf("T=%.2f C RH=%.2f %% lux=%.2f | faults(T/H/L)=%d/%d/%d\n",
            v.state.sample.temperature_c, v.state.sample.humidity_pct,
            v.state.sample.illuminance_lux, v.state.temperature_fault, v.state.humidity_fault, v.state.light_fault);
        printf("commanded light=%d fan=%d | RGB=%d MCP=%d SD=%d (no load feedback)\n",
            v.state.light_on, v.state.fan_on, v.led_ok, v.expander_ok, v.sd_ok);
        printf("fan on/off=%.1f/%.1f C | light on/off=%.1f/%.1f lux\n",
            v.state.config.fan_on_c, v.state.config.fan_off_c,
            v.state.config.light_on_lux, v.state.config.light_off_lux);
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "mode") &&
        (!strcmp(argv[2], "auto") || !strcmp(argv[2], "manual")))
        r.action = !strcmp(argv[2], "auto") ? HOME_AUTO : HOME_MANUAL_MODE;
    else if (argc == 3 && (!strcmp(argv[1], "light") || !strcmp(argv[1], "fan")) &&
        (!strcmp(argv[2], "on") || !strcmp(argv[2], "off"))) {
        r.action = !strcmp(argv[1], "fan") ? HOME_FAN_SET : HOME_LIGHT_SET;
        r.on = !strcmp(argv[2], "on");
    } else if (argc == 6 && !strcmp(argv[1], "thresholds")) {
        r.action = HOME_CONFIG;
        r.config = v.state.config;
        if (!number(argv[2], &r.config.fan_on_c) || !number(argv[3], &r.config.fan_off_c) ||
            !number(argv[4], &r.config.light_on_lux) || !number(argv[5], &r.config.light_off_lux) ||
            !home_config_valid(&r.config)) { puts("Invalid thresholds"); return 1; }
    } else if (argc == 5 && !strcmp(argv[1], "sim")) {
        if (!v.simulated) { puts("Simulation disabled in real-sensor build"); return 1; }
        r.action = HOME_SIM;
        r.sample.temperature_valid = r.sample.humidity_valid = r.sample.light_valid = true;
        if (!number(argv[2], &r.sample.temperature_c) || !number(argv[3], &r.sample.humidity_pct) ||
            !number(argv[4], &r.sample.illuminance_lux) ||
            r.sample.temperature_c < -40 || r.sample.temperature_c > 125 ||
            r.sample.humidity_pct < 0 || r.sample.humidity_pct > 100 ||
            r.sample.illuminance_lux < 0 || r.sample.illuminance_lux > 100000) {
            puts("Invalid simulation values"); return 1;
        }
    } else if (argc == 2 && !strcmp(argv[1], "fault") && v.simulated) r.action = HOME_SIM_FAULT;
    else if (argc == 2 && !strcmp(argv[1], "save")) r.action = HOME_SAVE;
    else if (argc == 3 && !strcmp(argv[1], "test")) {
        return diagnostics_submit(argv[2]) ? 0 : 1;
    } else {
        puts("home status | mode auto/manual | light on/off | fan on/off\n"
             "home thresholds <fan_on_C> <fan_off_C> <light_on_lux> <light_off_lux>\n"
             "home sim <C> <RH> <lux> | fault | save\n"
             "home test i2c/memory/tone/mic/camera/wifi\n"
             "Manual load commands switch to MANUAL. Save stores thresholds only.");
        return argc == 2 && !strcmp(argv[1], "help") ? 0 : 1;
    }
    bool accepted = home_submit(&r);
    puts(accepted ? "Queued; use home status to inspect the result" : "Queue full; retry");
    return accepted ? 0 : 1;
}

esp_err_t home_console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    cfg.prompt = "home>";
    cfg.max_cmdline_length = 192;
    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_uart(&uart_cfg, &cfg, &repl), TAG, "Console init");
    const esp_console_cmd_t cmd = {.command = "home", .help = "Smart home: home help", .func = command};
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&cmd), TAG, "Console command");
    return esp_console_start_repl(repl);
}
