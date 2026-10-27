// SPDX-License-Identifier: MIT
#include <stdint.h>
#include "sdkconfig.h"
#include "board_io.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"

static button_handle_t buttons[BSP_BUTTON_NUM];
#if CONFIG_HOME_SIMULATION
static led_indicator_handle_t leds[BSP_LED_NUM];
static bool led_ok, applied, last;
#endif
static void clicked(void *button, void *data)
{
    (void)button;
    home_request_t r = {.action = (home_action_t)(uintptr_t)data};
    if (!home_submit(&r)) ESP_LOGW("board", "Button queue full");
}
void board_io_init(void)
{
#if CONFIG_HOME_SIMULATION
    led_ok = bsp_led_indicator_create(leds, NULL, BSP_LED_NUM) == ESP_OK;
    if (led_ok) bsp_led_set(leds[0], false);
#endif
    esp_err_t err = bsp_iot_button_create(buttons, NULL, BSP_BUTTON_NUM);
    if (err != ESP_OK) { ESP_LOGW("board", "Buttons unavailable"); return; }
    const home_action_t map[BSP_BUTTON_NUM] = {
        HOME_AUTO, HOME_LIGHT_TOGGLE, HOME_FAN_TOGGLE, HOME_MANUAL_MODE
    };
    for (int i = 0; i < BSP_BUTTON_NUM; ++i) {
        err = iot_button_register_cb(buttons[i], BUTTON_SINGLE_CLICK, NULL,
                                    clicked, (void *)(uintptr_t)map[i]);
        if (err != ESP_OK) ESP_LOGW("board", "Button %d callback failed", i);
    }
}
void board_io_apply(const home_state_t *s)
{
#if CONFIG_HOME_SIMULATION
    if (led_ok && (!applied || last != s->light_on)) {
        applied = true;
        last = s->light_on;
        led_ok = bsp_led_set(leds[0], last) == ESP_OK;
    }
#else
    // Only explicit simulation calls this; real outputs belong to STM32.
    (void)s;
#endif
}
