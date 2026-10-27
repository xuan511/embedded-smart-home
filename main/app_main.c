// SPDX-License-Identifier: MIT
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "home_service.h"
#include "diagnostics.h"
#include "ui.h"

void app_main(void)
{
    // Preserve existing NVS on errors; never erase user data automatically.
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) ESP_LOGW("boot", "NVS unavailable: %s; settings remain volatile", esp_err_to_name(err));
    ESP_ERROR_CHECK(home_service_start());
    err = diagnostics_start();
    if (err != ESP_OK) ESP_LOGW("boot", "Self-test task: %s", esp_err_to_name(err));
    err = home_ui_start();
    if (err != ESP_OK) ESP_LOGW("boot", "LCD/touch: %s; serial console remains available", esp_err_to_name(err));
    ESP_ERROR_CHECK(home_console_start());
    ESP_LOGI("boot", "Ready. Type: home help. Source and link state are labelled; outputs are STM32 reports, without physical feedback.");
}
