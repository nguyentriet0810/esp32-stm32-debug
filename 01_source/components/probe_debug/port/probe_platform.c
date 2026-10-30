/*
 * SPDX-FileCopyrightText: 2026 Piotr Wasilewski
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Adapted from MCUViewer-probe Platform/Platform.cpp, reference commit 02755f4.
 */
/** @file probe_platform.c @brief ESP-IDF hooks required by Black Magic Debug. */
#include "probe_port.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "probe_state.h"
#include "general.h"
#include "platform.h"
static bool reset_asserted;
uint32_t target_clk_divider;
uint32_t platform_time_ms(void)
{
    return (uint32_t)((uint64_t)esp_timer_get_time() / UINT64_C(1000));
}
void platform_delay(uint32_t ms)
{
    if (ms != 0U)
    {
        vTaskDelay(pdMS_TO_TICKS(ms) + 1U);
    }
}
void platform_nrst_set_val(bool assert_reset)
{
#if CONFIG_PROBE_NRST_GPIO >= 0
    (void)gpio_set_level((gpio_num_t)CONFIG_PROBE_NRST_GPIO, assert_reset ? 0U : 1U);
#endif
    reset_asserted = assert_reset;
}
bool platform_nrst_get_val(void)
{
    return reset_asserted;
}
const char *platform_target_voltage(void)
{
    return "Unknown";
}
int platform_hwversion(void)
{
    return 1;
}
void platform_max_frequency_set(uint32_t frequency)
{
    probe_swd_set_frequency(frequency);
    probe_state_clock(probe_swd_get_frequency());
}
uint32_t platform_max_frequency_get(void)
{
    return probe_swd_get_frequency();
}
void platform_target_clk_output_enable(bool enable)
{
    probe_swd_drive(enable);
}
void debug_serial_send_stdout(const uint8_t *data, size_t length)
{
    if (data != NULL)
    {
        (void)fwrite(data, 1U, length, stdout);
        (void)fflush(stdout);
    }
}
bool platform_spi_init(spi_bus_e bus)
{
    (void)bus;
    return false;
}
bool platform_spi_deinit(spi_bus_e bus)
{
    (void)bus;
    return false;
}
bool platform_spi_chip_select(uint8_t select)
{
    (void)select;
    return false;
}
uint8_t platform_spi_xfer(spi_bus_e bus, uint8_t value)
{
    (void)bus;
    (void)value;
    return UINT8_MAX;
}
