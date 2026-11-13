idf_component_register(
    SRCS "app_main.c" "home_service.c" "home_usb.c" "home_network.c" "board_io.c" "diagnostics.c" "ui.c"
    INCLUDE_DIRS "."
    REQUIRES home_core home_remote home_gateway espressif__mqtt esp_http_server nvs_flash console esp_timer esp_wifi esp_event esp_netif
             esp_driver_i2c esp_psram fatfs)
