// SPDX-License-Identifier: MIT
#include "home_usb.h"
#include "sdkconfig.h"
#include "esp_timer.h"
#if !CONFIG_HOME_SIMULATION
#include "bsp/esp-bsp.h"
#include "usb/cdc_acm_host.h"
#include "usb/vcp_cp210x.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

typedef struct {
    hl_message_t message;
    uint32_t received_ms;
} received_frame_t;

static QueueHandle_t received;
static cdc_acm_dev_hdl_t device;
static hl_parser_t parser;
static portMUX_TYPE guard = portMUX_INITIALIZER_UNLOCKED;
static bool fault, installed;
static uint32_t next_open;

static void fail_link(void)
{
    portENTER_CRITICAL(&guard);
    fault = true;
    portEXIT_CRITICAL(&guard);
}

static bool rx(const uint8_t *data, size_t length, void *arg)
{
    (void)arg;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    for (size_t i = 0; i < length; ++i) {
        received_frame_t frame = {.received_ms = now};
        if (hl_parser_feed(&parser, (char)data[i], now, &frame.message) &&
            xQueueSend(received, &frame, 0) != pdTRUE) fail_link();
    }
    return true;
}

static void event(const cdc_acm_host_dev_event_data_t *data, void *arg)
{
    (void)arg;
    if (data->type == CDC_ACM_HOST_DEVICE_DISCONNECTED || data->type == CDC_ACM_HOST_ERROR)
        fail_link();
}

esp_err_t home_usb_start(void)
{
    received = xQueueCreate(16, sizeof(received_frame_t));
    if (!received) return ESP_ERR_NO_MEM;
    // Native S31 USB Host via BSP, without GPIO19/20 remapping.
    esp_err_t err = bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true);
    if (err != ESP_OK) return err;
    const cdc_acm_host_driver_config_t cfg = {
        .driver_task_stack_size = 4096, .driver_task_priority = 10, .xCoreID = 0
    };
    err = cdc_acm_host_install(&cfg);
    installed = err == ESP_OK;
    return err;
}

bool home_usb_poll(hl_message_t *message, bool *attached, uint32_t *received_ms)
{
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    portENTER_CRITICAL(&guard);
    bool failed = fault;
    fault = false;
    portEXIT_CRITICAL(&guard);
    if (failed && device) {
        cdc_acm_host_close(device);
        device = NULL;
        xQueueReset(received);
        next_open = now + 1000;
        ESP_LOGW("home_usb", "USB disconnected/error; pending operations discarded");
    }
    if (installed && !device && (int32_t)(now - next_open) >= 0) {
        hl_parser_init(&parser);
        xQueueReset(received);
        const cdc_acm_host_device_config_t cfg = {
            .connection_timeout_ms = 100, .out_buffer_size = 256, .in_buffer_size = 256,
            .event_cb = event, .data_cb = rx
        };
        esp_err_t err = cp210x_vcp_open(CP210X_PID, 0, &cfg, &device);
        if (err == ESP_OK) {
            const cdc_acm_line_coding_t coding = {
                .dwDTERate = 115200, .bCharFormat = 0, .bParityType = 0, .bDataBits = 8
            };
            err = cdc_acm_host_line_coding_set(device, &coding);
            if (err == ESP_OK) err = cdc_acm_host_set_control_line_state(device, false, false);
            if (err != ESP_OK) fail_link();
            else ESP_LOGI("home_usb", "CP210x 10C4:EA60 opened, 115200 8N1; awaiting STM32");
        } else device = NULL;
        next_open = now + 1000;
    }
    *attached = device != NULL && !failed;
    received_frame_t frame;
    if (!*attached || xQueueReceive(received, &frame, 0) != pdTRUE) return false;
    *message = frame.message;
    *received_ms = frame.received_ms;
    return true;
}

bool home_usb_send(const hl_message_t *message)
{
    char frame[HL_FRAME_MAX + 1];
    size_t length = hl_encode(message, frame, sizeof(frame));
    if (!device || !length) return false;
    if (cdc_acm_host_data_tx_blocking(device, (const uint8_t *)frame, length, 150) == ESP_OK)
        return true;
    fail_link();
    return false;
}
#else
esp_err_t home_usb_start(void) { return ESP_OK; }
bool home_usb_poll(hl_message_t *m, bool *attached, uint32_t *received_ms)
{ (void)m; (void)received_ms; *attached = false; return false; }
bool home_usb_send(const hl_message_t *m) { (void)m; return false; }
#endif
