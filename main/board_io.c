// SPDX-License-Identifier: MIT
#include <stdint.h>
#include "sdkconfig.h"
#include "board_io.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"

static const char *TAG = "board";
static led_indicator_handle_t leds[BSP_LED_NUM];
static button_handle_t buttons[BSP_BUTTON_NUM];
static bool led_ok, output_ok;
static bool led_applied, last_light;
#if CONFIG_HOME_MCP_OUTPUTS
static i2c_master_dev_handle_t mcp;
static esp_err_t reg_write(uint8_t address, uint8_t value)
{
    uint8_t data[] = {address, value};
    return i2c_master_transmit(mcp, data, sizeof(data), 100);
}
#endif

static void button_clicked(void *button, void *user_data)
{
    (void)button;
    const home_action_t action = (home_action_t)(uintptr_t)user_data;
    home_request_t r = {.action = action};
    if (!home_submit(&r)) ESP_LOGW(TAG, "Button command queue full");
}

void board_io_init(void)
{
    led_ok = bsp_led_indicator_create(leds, NULL, BSP_LED_NUM) == ESP_OK;
    if (led_ok) bsp_led_set(leds[0], false);
    esp_err_t err = bsp_iot_button_create(buttons, NULL, BSP_BUTTON_NUM);
    if (err == ESP_OK) {
        // BSP MODE is the board's PLAY key. ADC thresholds stay inside the BSP.
        const home_action_t map[BSP_BUTTON_NUM] = {
            HOME_AUTO, HOME_LIGHT_TOGGLE, HOME_FAN_TOGGLE, HOME_MANUAL_MODE
        };
        for (int i = 0; i < BSP_BUTTON_NUM; ++i) {
            err = iot_button_register_cb(buttons[i], BUTTON_SINGLE_CLICK, NULL,
                                         button_clicked, (void *)(uintptr_t)map[i]);
            if (err != ESP_OK) ESP_LOGW(TAG, "Button %d callback: %s", i, esp_err_to_name(err));
        }
    } else ESP_LOGW(TAG, "Buttons unavailable: %s", esp_err_to_name(err));
#if CONFIG_HOME_MCP_OUTPUTS
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x20, .scl_speed_hz = 100000
    };
    err = i2c_master_bus_add_device(bsp_i2c_get_handle(), &cfg, &mcp);
    // BANK=0 reset state required. Set output latch OFF before changing direction.
    if (err == ESP_OK) err = reg_write(0x14, 0x00); // OLATA
    if (err == ESP_OK) err = reg_write(0x00, 0xfc); // IODIRA: PA0/1 out, others input
    output_ok = err == ESP_OK;
    if (!output_ok) ESP_LOGW(TAG, "MCP23017 unavailable: %s", esp_err_to_name(err));
#endif
}

void board_io_apply(const home_state_t *s)
{
    if (led_ok && (!led_applied || s->light_on != last_light)) {
        esp_err_t err = bsp_led_set(leds[0], s->light_on);
        led_applied = true;
        last_light = s->light_on;
        if (err != ESP_OK) { led_ok = false; ESP_LOGW(TAG, "RGB LED failed"); }
    }
#if CONFIG_HOME_MCP_OUTPUTS
    if (mcp) {
        // Recover direction/latch configuration as well as data after a bus error.
        esp_err_t err = ESP_OK;
        if (!output_ok) {
            err = reg_write(0x14, 0x00);
            if (err == ESP_OK) err = reg_write(0x00, 0xfc);
        }
        if (err == ESP_OK)
            err = reg_write(0x14, (s->light_on ? 1 : 0) | (s->fan_on ? 2 : 0));
        output_ok = err == ESP_OK;
    }
#else
    (void)s;
#endif
}

void board_io_health(bool *led, bool *expander)
{
    *led = led_ok;
    *expander = output_ok;
}
