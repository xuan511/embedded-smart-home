// SPDX-License-Identifier: MIT
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#ifndef CONFIG_HOME_SIMULATION
#define CONFIG_HOME_SIMULATION 0
#endif
#include "home_service.h"
#include "home_remote.h"
#include "home_usb.h"
#include "board_io.h"
#include "diagnostics.h"
#include "bsp/esp-bsp.h"
#include "esp_console.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

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
    // Age again at the reader: even a slow SD write must not freeze the LCD at OK.
    uint64_t now = home_now_ms();
    uint64_t elapsed = now >= s->uptime_ms ? now - s->uptime_ms : UINT32_MAX;
    uint64_t age = (uint64_t)s->sample_age_ms + elapsed;
    s->sample_age_ms = age > INT32_MAX ? INT32_MAX : (uint32_t)age;
    s->uptime_ms = now;
    if (!s->simulated) {
        if ((uint64_t)s->link_age_ms + elapsed >= 3000) s->online = false;
        s->stale |= !s->online || age >= s->state.config.stale_ms;
        s->outputs_known &= s->online;
        if (!s->online) s->synced = false;
        s->state.temperature_fault |= s->stale;
        s->state.humidity_fault |= s->stale;
        s->state.light_fault |= s->stale;
    }
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

static bool log_csv(const home_snapshot_t *v)
{
    FILE *f = fopen(BSP_SD_MOUNT_POINT "/home.csv", "a");
    if (!f) return false;
    const home_state_t *s = &v->state;
    bool ok = true;
    if (ftell(f) == 0)
        ok = fprintf(f, "uptime_ms,simulation,source,online,synced,stale,auto,temp_c,rh_pct,lux,temp_ok,rh_ok,light_ok,reported_light,reported_fan,outputs_known\n") > 0;
    ok &= fprintf(f, "%" PRIu64 ",%d,%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%d\n",
        v->uptime_ms, v->simulated, v->source, v->online, v->synced, v->stale, s->automatic,
        s->sample.temperature_c, s->sample.humidity_pct, s->sample.illuminance_lux,
        !s->temperature_fault, !s->humidity_fault, !s->light_fault,
        s->light_on, s->fan_on, v->outputs_known) > 0;
    if (fclose(f) != 0) ok = false;
    return ok;
}

static void controller_task(void *arg)
{
    (void)arg;
    home_config_t cfg = home_default_config();
    load_config(&cfg);
    home_remote_t remote;
    home_remote_init(&remote, &cfg);
    home_state_t sim_state;
    home_init(&sim_state, &cfg);
    home_sample_t sim = {.temperature_c = 25, .humidity_pct = 50, .illuminance_lux = 300,
        .temperature_valid = true, .humidity_valid = true, .light_valid = true};
    uint64_t next_sample = 0, next_log = 10000, next_status = 0;
    bool sd_ok = false, was_attached = false;
    int32_t user_seq = 0, user_session = 0;
    char notice[96] = "No operation requested; load feedback unavailable";
#if CONFIG_HOME_SD_LOG
    sd_ok = bsp_sdcard_mount() == ESP_OK;
    ESP_LOGI(TAG, "SD log: %s", sd_ok ? "ready" : "unavailable");
#endif
    for (;;) {
        uint64_t now = home_now_ms();
        if (!CONFIG_HOME_SIMULATION) {
            hl_message_t message;
            bool attached = false;
            uint32_t received_ms = 0;
            bool got = home_usb_poll(&message, &attached, &received_ms);
            now = home_now_ms();
            if (attached && !was_attached)
                home_remote_attach(&remote, (int32_t)(esp_random() & 0x7fffffffu), (uint32_t)now);
            if (!attached && was_attached) home_remote_detach(&remote);
            was_attached = attached;
            while (got) {
                now = home_now_ms();
                bool user_pending = user_seq && remote.pending &&
                    remote.session == user_session && remote.pending_seq == user_seq;
                home_remote_receive_at(&remote, &message, received_ms, (uint32_t)now);
                if (user_pending && !remote.pending && message.type == HL_STATUS &&
                    message.v[0] == user_session && message.v[1] == user_seq) {
                    snprintf(notice, sizeof(notice), "STM32 ACK seq=%ld result=%ld (0=OK); no physical feedback",
                        (long)user_seq, (long)message.v[2]);
                    user_seq = 0;
                }
                got = home_usb_poll(&message, &attached, &received_ms);
            }
            now = home_now_ms();
            if (home_remote_poll(&remote, (uint32_t)now, &message)) home_usb_send(&message);
            if (user_seq && (!remote.attached || remote.session != user_session)) {
                snprintf(notice, sizeof(notice), "Request timed out/disconnected; result UNKNOWN; not replayed");
                user_seq = 0;
            }
        }
        home_request_t r;
        if (xQueueReceive(requests, &r, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (r.action == HOME_CONFIG) {
                if (home_config_valid(&r.config)) {
                    home_remote_config(&remote, &r.config);
                    sim_state.config = r.config;
                    snprintf(notice, sizeof(notice), "Thresholds changed in RAM; sync pending; home save persists");
                }
            } else if (r.action == HOME_SAVE) {
                esp_err_t err = save_config(&remote.desired);
                snprintf(notice, sizeof(notice), "NVS save: %s", esp_err_to_name(err));
                ESP_LOGI(TAG, "%s", notice);
            } else if (CONFIG_HOME_SIMULATION) {
                switch (r.action) {
                case HOME_AUTO: sim_state.automatic = true; break;
                case HOME_MANUAL_MODE: sim_state.automatic = false; break;
                case HOME_LIGHT_TOGGLE: home_manual(&sim_state, false, !sim_state.light_on); break;
                case HOME_FAN_TOGGLE: home_manual(&sim_state, true, !sim_state.fan_on); break;
                case HOME_LIGHT_SET: home_manual(&sim_state, false, r.on); break;
                case HOME_FAN_SET: home_manual(&sim_state, true, r.on); break;
                case HOME_SIM: sim = r.sample; next_sample = 0; break;
                case HOME_SIM_FAULT:
                    sim.temperature_valid = sim.humidity_valid = sim.light_valid = false;
                    next_sample = 0; break;
                default: break;
                }
                snprintf(notice, sizeof(notice), "SIMULATION operation applied; no STM32/load feedback");
            } else {
                hl_type_t type = HL_SET;
                int which = 0, value = 0;
                bool valid = true;
                switch (r.action) {
                case HOME_AUTO: type = HL_MODE; which = 1; break;
                case HOME_MANUAL_MODE: type = HL_MODE; which = 0; break;
                case HOME_LIGHT_TOGGLE: value = !remote.reported.light_on; break;
                case HOME_FAN_TOGGLE: which = 1; value = !remote.reported.fan_on; break;
                case HOME_LIGHT_SET: value = r.on; break;
                case HOME_FAN_SET: which = 1; value = r.on; break;
                default: valid = false; break;
                }
                hl_message_t message;
                if (valid && home_remote_request(&remote, type, which, value, (uint32_t)now, &message)) {
                    user_seq = message.v[1];
                    user_session = remote.session;
                    snprintf(notice, sizeof(notice), "REQUEST %s %d/%d seq=%ld; awaiting STM32 ACK",
                        type == HL_MODE ? "MODE" : "SET", which, value, (long)user_seq);
                    home_usb_send(&message);
                } else snprintf(notice, sizeof(notice), "Request REJECTED: offline/syncing/busy; retry after READY");
                ESP_LOGI(TAG, "%s", notice);
            }
        }
        now = home_now_ms();
        home_snapshot_t view = {.uptime_ms = now, .sd_ok = sd_ok,
            .simulated = CONFIG_HOME_SIMULATION};
        if (CONFIG_HOME_SIMULATION) {
            if (now >= next_sample) {
                sim.timestamp_ms = now;
                home_accept_sample(&sim_state, &sim);
                next_sample = now + 1000;
            }
            home_tick(&sim_state, now);
            board_io_apply(&sim_state);
            view.state = sim_state;
            view.synced = true;
            view.stale = false;
        } else {
            home_remote_view(&remote, (uint32_t)now, &view.state, &view.online, &view.stale);
            view.synced = remote.synced;
            view.pending = remote.pending;
            view.source = remote.source;
            view.stage = remote.stage;
            view.outputs_known = view.online;
            view.link_age_ms = (uint32_t)((uint32_t)now - remote.last_rx);
            uint64_t age = (uint64_t)remote.sample_age + (uint32_t)((uint32_t)now - remote.sample_received);
            view.sample_age_ms = !remote.have_status || age > INT32_MAX ? INT32_MAX : (uint32_t)age;
        }
        snprintf(view.request_status, sizeof(view.request_status), "%s", notice);
        if (sd_ok && now >= next_log) {
            sd_ok = log_csv(&view);
            next_log = now + 10000;
            if (!sd_ok) ESP_LOGW(TAG, "SD write failed; logging stopped until reboot");
        }
        portENTER_CRITICAL(&count_lock);
        view.rejected_requests = rejected;
        portEXIT_CRITICAL(&count_lock);
        xSemaphoreTake(snapshot_lock, portMAX_DELAY);
        published = view;
        xSemaphoreGive(snapshot_lock);
        if (now >= next_status) {
            ESP_LOGI(TAG, "%s stage=%d online=%d synced=%d stale=%d | reported light=%d fan=%d faults=%d/%d/%d",
                view.simulated ? "SIMULATION" : (view.source == 1 ? "STM32 TEST" : "STM32"),
                view.stage, view.online, view.synced, view.stale,
                view.state.light_on, view.state.fan_on, view.state.temperature_fault,
                view.state.humidity_fault, view.state.light_fault);
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
    published.simulated = CONFIG_HOME_SIMULATION;
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "BSP I2C failed");
    board_io_init();
    esp_err_t err = home_usb_start();
    if (err != ESP_OK) ESP_LOGE(TAG, "USB unavailable: %s; remain OFFLINE", esp_err_to_name(err));
    return xTaskCreate(controller_task, "home_service", 7168, NULL, 5, NULL) == pdPASS
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
        printf("%s stage=%d online=%d synced=%d stale=%d pending=%d | %s uptime=%" PRIu64 "ms rejected=%" PRIu32 "\n",
            v.simulated ? "SIMULATION" : (v.source == 1 ? "STM32 TEST DATA" : "STM32 REAL/WAIT"),
            v.stage, v.online, v.synced, v.stale, v.pending, v.state.automatic ? "AUTO" : "MANUAL",
            v.uptime_ms, v.rejected_requests);
        printf("Last T=%.2f C RH=%.2f %% lux=%.2f | faults(T/H/L)=%d/%d/%d age=%" PRIu32 "ms source=%d\n",
            v.state.sample.temperature_c, v.state.sample.humidity_pct, v.state.sample.illuminance_lux,
            v.state.temperature_fault, v.state.humidity_fault, v.state.light_fault, v.sample_age_ms, v.source);
        printf("%s light=%d fan=%d | outputs_known=%d SD=%d (NO physical feedback)\n",
            v.simulated ? "SIMULATED commands" : "STM32 reported",
            v.state.light_on, v.state.fan_on, v.outputs_known, v.sd_ok);
        printf("Desired fan on/off=%.2f/%.2f C light on/off=%.1f/%.1f lux\n%s\n",
            v.state.config.fan_on_c, v.state.config.fan_off_c,
            v.state.config.light_on_lux, v.state.config.light_off_lux, v.request_status);
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "mode") == 0 &&
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
        r.config.fan_on_c = roundf(r.config.fan_on_c * 100) / 100;
        r.config.fan_off_c = roundf(r.config.fan_off_c * 100) / 100;
        r.config.light_on_lux = roundf(r.config.light_on_lux * 10) / 10;
        r.config.light_off_lux = roundf(r.config.light_off_lux * 10) / 10;
        if (!home_config_valid(&r.config)) { puts("Threshold hysteresis disappears after wire precision rounding"); return 1; }
    } else if (argc == 5 && !strcmp(argv[1], "sim")) {
        if (!v.simulated) { puts("Simulation only available in explicit simulation build"); return 1; }
        r.action = HOME_SIM;
        r.sample.temperature_valid = r.sample.humidity_valid = r.sample.light_valid = true;
        if (!number(argv[2], &r.sample.temperature_c) || !number(argv[3], &r.sample.humidity_pct) ||
            !number(argv[4], &r.sample.illuminance_lux) || r.sample.temperature_c < -40 ||
            r.sample.temperature_c > 125 || r.sample.humidity_pct < 0 || r.sample.humidity_pct > 100 ||
            r.sample.illuminance_lux < 0 || r.sample.illuminance_lux > 100000) {
            puts("Invalid simulation values"); return 1;
        }
    } else if (argc == 2 && !strcmp(argv[1], "fault") && v.simulated) r.action = HOME_SIM_FAULT;
    else if (argc == 2 && !strcmp(argv[1], "save")) r.action = HOME_SAVE;
    else if (argc == 3 && !strcmp(argv[1], "test")) return diagnostics_submit(argv[2]) ? 0 : 1;
    else {
        puts("home status | mode auto/manual | light on/off | fan on/off\n"
             "home thresholds <fan_on_C> <fan_off_C> <light_on_lux> <light_off_lux> | save\n"
             "home sim <C> <RH> <lux> | fault (explicit simulation only)\n"
             "home test i2c/memory/tone/mic/camera/wifi (optional diagnostics)\n"
             "SET changes STM32 to MANUAL. Queued is not an ACK. No physical load feedback.");
        return argc == 2 && !strcmp(argv[1], "help") ? 0 : 1;
    }
    bool accepted = home_submit(&r);
    puts(accepted ? "Queued locally; inspect home status for STM32 ACK or rejection" : "Queue full; retry");
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
