// SPDX-License-Identifier: MIT
#include "sdkconfig.h"
#include "sensors.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_HOME_REAL_SENSORS
static i2c_master_dev_handle_t sht, bh;
static const char *TAG = "sensors";

static void add_device(unsigned address, i2c_master_dev_handle_t *handle)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address, .scl_speed_hz = 100000,
    };
    esp_err_t err = i2c_master_bus_add_device(bsp_i2c_get_handle(), &cfg, handle);
    if (err != ESP_OK) ESP_LOGW(TAG, "0x%02x: %s", address, esp_err_to_name(err));
}
#endif

void sensors_init(void)
{
#if CONFIG_HOME_REAL_SENSORS
    add_device(CONFIG_HOME_SHT_ADDRESS, &sht);
    add_device(CONFIG_HOME_BH_ADDRESS, &bh);
#endif
}

home_sample_t sensors_read(uint64_t now)
{
    home_sample_t p = {.timestamp_ms = now};
#if CONFIG_HOME_REAL_SENSORS
    // Single-shot, high repeatability, no clock stretching. Check BOTH CRC bytes.
    const uint8_t measure[] = {0x24, 0x00};
    uint8_t raw[6];
    if (sht && i2c_master_transmit(sht, measure, sizeof(measure), 100) == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (i2c_master_receive(sht, raw, sizeof(raw), 100) == ESP_OK &&
            home_sht_decode(raw, &p.temperature_c, &p.humidity_pct)) {
            p.temperature_valid = p.humidity_valid = true;
        }
    }
    // One-time high-resolution BH1750 measurement (max. 180 ms conversion).
    const uint8_t cmd = 0x20;
    if (bh && i2c_master_transmit(bh, &cmd, 1, 100) == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(180));
        if (i2c_master_receive(bh, raw, 2, 100) == ESP_OK) {
            p.illuminance_lux = ((unsigned)raw[0] * 256 + raw[1]) / 1.2f;
            p.light_valid = true;
        }
    }
#else
    // Simulation is produced by home_service, never presented as a real sensor.
    (void)now;
#endif
    return p;
}
