// SPDX-License-Identifier: MIT
#include "board.h"
#include "stm32_controller.h"

static void send_message(const hl_message_t *message)
{
    char bytes[HL_FRAME_MAX + 1];
    const size_t length = hl_encode(message, bytes, sizeof(bytes));
    if (length) board_uart_write(bytes, length);
}

int main(void)
{
    board_init();
    stm32_controller_t controller;
    sc_init(&controller, STAGE);
    hl_parser_t parser;
    hl_parser_init(&parser);
    uint32_t last_status = board_millis();
#if STAGE >= 2
    uint32_t last_sample = board_millis() - 1000u;
#endif
    uint32_t previous_ms = board_millis();
    uint64_t now_ms = 0; // Extend wrapping 32-bit uptime for the portable controller.
    for (;;) {
        uint32_t raw = board_millis();
        now_ms += (uint32_t)(raw - previous_ms); previous_ms = raw;
        if (board_uart_lost_bytes()) {
            hl_parser_init(&parser); parser.dropping = true; // Drop truncated frame through LF.
        }
        char byte;
        hl_message_t request, reply;
        while (board_uart_read(&byte))
            if (hl_parser_feed(&parser, byte, raw, &request) && sc_receive(&controller, &request, now_ms, &reply)) {
                board_outputs(controller.state.light_on, controller.state.fan_on);
                send_message(&reply);
            }
        hl_parser_expire(&parser, raw);
#if STAGE >= 2
        if ((uint32_t)(raw - last_sample) >= 1000u) {
            last_sample = raw;
#if STAGE == 2
            home_sample_t sample = {.temperature_c = 30, .humidity_pct = 55, .illuminance_lux = 50,
                .temperature_valid = true, .humidity_valid = true, .light_valid = true, .timestamp_ms = now_ms};
            sc_sample(&controller, &sample, 1);
#else
            home_sample_t sample = board_sensors_read();
            // Driver uses wrapping hardware millis; controller always uses extended uptime.
            raw = board_millis(); now_ms += (uint32_t)(raw - previous_ms); previous_ms = raw;
            sample.timestamp_ms = now_ms;
            sc_sample(&controller, &sample, 2);
#endif
        }
#endif
        sc_tick(&controller, now_ms);
        board_outputs(controller.state.light_on, controller.state.fan_on);
        if (controller.session && (uint32_t)(raw - last_status) >= 1000u) {
            last_status = raw;
            sc_status(&controller, now_ms, 0, 0, &reply);
            send_message(&reply);
        }
        __asm volatile ("wfi"); // SysTick and UART IRQ wake the small cooperative loop.
    }
}
