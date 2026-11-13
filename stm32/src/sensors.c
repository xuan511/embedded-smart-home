// SPDX-License-Identifier: MIT
#include "board.h"

home_sample_t board_sensors_read(void)
{
    home_sample_t sample = {0};
    const uint8_t sht_command[] = {0x24, 0x00}; // SHT3x-DIS high repeatability, no stretch.
    uint8_t sht[6];
    if (board_i2c_write(0x44, sht_command, sizeof(sht_command))) {
        board_delay_ms(16); // Datasheet maximum high-repeatability conversion 15 ms.
        if (board_i2c_read(0x44, sht, sizeof(sht)) &&
            home_sht_decode(sht, &sample.temperature_c, &sample.humidity_pct))
            sample.temperature_valid = sample.humidity_valid = true;
    }
    // BH1750 one-time H-resolution: powers down after each conversion.
    const uint8_t power_on = 0x01, measure = 0x20;
    uint8_t bh[2];
    if (board_i2c_write(0x23, &power_on, 1) && board_i2c_write(0x23, &measure, 1)) {
        board_delay_ms(180); // H-resolution measurement max at default MTreg=69.
        if (board_i2c_read(0x23, bh, sizeof(bh))) {
            sample.illuminance_lux = ((unsigned)bh[0] * 256u + bh[1]) / 1.2f;
            sample.light_valid = true;
        }
    }
    sample.timestamp_ms = board_millis();
    return sample;
}
