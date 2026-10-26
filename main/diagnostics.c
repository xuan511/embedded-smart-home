// SPDX-License-Identifier: MIT
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "diagnostics.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#if CONFIG_HOME_CAMERA
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "linux/videodev2.h"
#include "esp_video_device.h"
#endif

static const char *TAG = "selftest";
static const char *names[] = {"i2c", "memory", "tone", "mic", "camera", "wifi"};
static QueueHandle_t queue;
static SemaphoreHandle_t message_lock;
static char last_message[128] = "Self-test idle";

static void message(const char *text)
{
    xSemaphoreTake(message_lock, portMAX_DELAY);
    snprintf(last_message, sizeof(last_message), "%s", text);
    xSemaphoreGive(message_lock);
    ESP_LOGI(TAG, "%s", text);
}

void diagnostics_message(char *buffer, size_t length)
{
    if (!message_lock) { snprintf(buffer, length, "Self-test unavailable"); return; }
    xSemaphoreTake(message_lock, portMAX_DELAY);
    snprintf(buffer, length, "%s", last_message);
    xSemaphoreGive(message_lock);
}

static esp_err_t memory_test(void)
{
    size_t size = 64 * 1024;
    uint8_t *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) return ESP_ERR_NO_MEM;
    for (size_t i = 0; i < size; ++i) p[i] = (uint8_t)(i ^ (i >> 8));
    esp_err_t err = ESP_OK;
    for (size_t i = 0; i < size; ++i)
        if (p[i] != (uint8_t)(i ^ (i >> 8))) { err = ESP_FAIL; break; }
    heap_caps_free(p);
    ESP_LOGI(TAG, "PSRAM total=%u free=%u; tested 64 KiB (not exhaustive)",
        (unsigned)esp_psram_get_size(), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return err;
}

static esp_err_t i2c_test(void)
{
    unsigned found = 0;
    for (unsigned address = 0x08; address < 0x78; ++address) {
        if (i2c_master_probe(bsp_i2c_get_handle(), address, 20) == ESP_OK) {
            ESP_LOGI(TAG, "I2C ACK at 0x%02x", address);
            ++found;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    ESP_LOGI(TAG, "I2C devices responding: %u", found);
    return found ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static esp_err_t audio_test(bool record)
{
    static esp_codec_dev_handle_t speaker, microphone;
    esp_codec_dev_handle_t *dev = record ? &microphone : &speaker;
    if (!*dev) *dev = record ? bsp_audio_codec_microphone_init() : bsp_audio_codec_speaker_init();
    if (!*dev) return ESP_FAIL;
    // Official BSP recommends 16 kHz because MCLK is not connected on this board.
    esp_codec_dev_sample_info_t fmt = {.sample_rate = 16000, .channel = 2, .bits_per_sample = 16};
    int ret = esp_codec_dev_open(*dev, &fmt);
    if (ret != ESP_CODEC_DEV_OK) return ESP_FAIL;
    ret = record ? esp_codec_dev_set_in_gain(*dev, 12.0f) : esp_codec_dev_set_out_vol(*dev, 20);
    int16_t pcm[320]; // 160 stereo frames = 10 ms
    unsigned peak_left = 0, peak_right = 0;
    for (unsigned block = 0; block < 100 && ret == ESP_CODEC_DEV_OK; ++block) {
        if (record) {
            ret = esp_codec_dev_read(*dev, pcm, sizeof(pcm));
            if (ret != ESP_CODEC_DEV_OK) break;
            for (unsigned n = 0; n < 320; ++n) {
                int sample = pcm[n];
                unsigned amplitude = sample < 0 ? (unsigned)-sample : (unsigned)sample;
                unsigned *peak = (n & 1) ? &peak_right : &peak_left;
                if (amplitude > *peak) *peak = amplitude;
            }
        } else {
            // Low-amplitude 500 Hz square wave, same samples in both channels.
            for (unsigned n = 0; n < 160; ++n)
                pcm[2 * n] = pcm[2 * n + 1] = (n % 32 < 16) ? 1200 : -1200;
            ret = esp_codec_dev_write(*dev, pcm, sizeof(pcm));
        }
    }
    int close_ret = esp_codec_dev_close(*dev);
    if (record) ESP_LOGI(TAG, "Mic peak L/R=%u/%u (raw amplitude; not calibrated dB, not recognition)", peak_left, peak_right);
    return ret == ESP_CODEC_DEV_OK && close_ret == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

static esp_err_t camera_test(void)
{
#if CONFIG_HOME_CAMERA
    static bool initialized;
    if (!initialized) {
        esp_err_t err = bsp_camera_start(NULL);
        if (err != ESP_OK) return err;
        initialized = true;
    }
    int fd = open(BSP_CAMERA_DEVICE, O_RDONLY);
    if (fd < 0) return ESP_FAIL;
    struct v4l2_capability cap = {0};
    struct v4l2_format fmt = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    bool ok = ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0 && ioctl(fd, VIDIOC_G_FMT, &fmt) == 0;
    if (ok) ESP_LOGI(TAG, "Camera %s: %" PRIu32 "x%" PRIu32 " format=0x%08" PRIx32,
        cap.card, fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.pixelformat);
    close(fd);
    return ok ? ESP_OK : ESP_FAIL;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static esp_err_t wifi_test(void)
{
    static bool ready;
    if (!ready) {
        ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
        esp_err_t err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
        if (!esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init");
        ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi storage");
        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode");
        ready = true;
    }
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
    esp_err_t err = esp_wifi_scan_start(NULL, true);
    if (err == ESP_OK) {
        wifi_ap_record_t records[8];
        uint16_t count = 8;
        err = esp_wifi_scan_get_ap_records(&count, records);
        for (unsigned i = 0; err == ESP_OK && i < count; ++i)
            ESP_LOGI(TAG, "AP %.32s RSSI=%d channel=%u", records[i].ssid, records[i].rssi, records[i].primary);
    }
    // Does not connect, upload, or store credentials. Stop the radio after the test.
    esp_wifi_clear_ap_list();
    esp_wifi_stop();
    return err;
}

static void worker(void *arg)
{
    (void)arg;
    for (;;) {
        unsigned which;
        if (xQueueReceive(queue, &which, portMAX_DELAY) != pdTRUE) continue;
        char text[128];
        snprintf(text, sizeof(text), "Running %s", names[which]);
        message(text);
        esp_err_t err;
        switch (which) {
        case 0: err = i2c_test(); break;
        case 1: err = memory_test(); break;
        case 2: err = audio_test(false); break;
        case 3: err = audio_test(true); break;
        case 4: err = camera_test(); break;
        default: err = wifi_test(); break;
        }
        snprintf(text, sizeof(text), "%s: %s", names[which], esp_err_to_name(err));
        message(text);
    }
}

bool diagnostics_submit(const char *name)
{
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (!strcmp(name, names[i])) return queue && xQueueSend(queue, &i, 0) == pdTRUE;
    ESP_LOGW(TAG, "Unknown test: %s", name);
    return false;
}

esp_err_t diagnostics_start(void)
{
    message_lock = xSemaphoreCreateMutex();
    queue = xQueueCreate(2, sizeof(unsigned));
    if (!message_lock || !queue) return ESP_ERR_NO_MEM;
    return xTaskCreate(worker, "home_selftest", 8192, NULL, 3, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
