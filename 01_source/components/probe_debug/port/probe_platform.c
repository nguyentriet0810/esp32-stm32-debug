/*
 * SPDX-FileCopyrightText: 2026 Piotr Wasilewski
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Adapted from MCUViewer-probe Platform/Platform.cpp, reference commit 02755f4.
 */
/**
 * @file probe_platform.c
 * @brief ESP-IDF implementations of Black Magic Debug platform hooks.
 *
 * Provides timing, open-drain target reset and nominal SWD frequency. Target voltage sensing and
 * backend SPI transport are not implemented by this port.
 */
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
/**
 * @brief Read the platform monotonic timer as wrapping 32-bit milliseconds.
 * @return Milliseconds since boot, modulo 2^32.
 */
uint32_t platform_time_ms(void)
{
    return (uint32_t)((uint64_t)esp_timer_get_time() / UINT64_C(1000));
}
/**
 * @brief Yield for a nonzero millisecond delay rounded by the FreeRTOS tick rate.
 * @param[in] ms Delay in milliseconds; zero returns immediately.
 * @note Adds one tick for nonzero requests. Task context only.
 */
void platform_delay(uint32_t ms)
{
    if (ms != 0U)
    {
        vTaskDelay(pdMS_TO_TICKS(ms) + 1U);
    }
}
/**
 * @brief Drive active-low target reset when the reset GPIO is configured.
 * @param[in] assert_reset True to pull nRST low; false to release the open-drain output.
 * @note Stores the requested state even when the reset GPIO is disabled.
 */
void platform_nrst_set_val(bool assert_reset)
{
#if CONFIG_PROBE_NRST_GPIO >= 0
    (void)gpio_set_level((gpio_num_t)CONFIG_PROBE_NRST_GPIO, assert_reset ? 0U : 1U);
#endif
    reset_asserted = assert_reset;
}
/**
 * @brief Return the last requested reset state, rather than sampling target voltage.
 * @return true when reset was last requested asserted.
 */
bool platform_nrst_get_val(void)
{
    return reset_asserted;
}
/**
 * @brief Report unavailable target voltage sensing to the backend.
 * @return A static string; the caller must not free it.
 */
const char *platform_target_voltage(void)
{
    return "Unknown";
}
/**
 * @brief Supply the fixed platform revision expected by BMD.
 * @return Platform hardware revision 1.
 */
int platform_hwversion(void)
{
    return 1;
}
/**
 * @brief Apply a clamped SWD frequency and publish its nominal result.
 * @param[in] frequency Requested SWD clock in Hz.
 * @pre Called by the core-1 debug owner.
 */
void platform_max_frequency_set(uint32_t frequency)
{
    probe_swd_set_frequency(frequency);
    probe_state_clock(probe_swd_get_frequency());
}
/**
 * @brief Return the current nominal SWD clock.
 * @return Selected frequency in Hz.
 */
uint32_t platform_max_frequency_get(void)
{
    return probe_swd_get_frequency();
}
/**
 * @brief Forward backend output ownership to the SWD GPIO port.
 * @param[in] enable True to drive SWD; false to release the outputs.
 */
void platform_target_clk_output_enable(bool enable)
{
    probe_swd_drive(enable);
}
/**
 * @brief Forward backend diagnostic bytes to the configured ESP-IDF console.
 * @param[in] data Borrowed byte buffer; NULL produces no output.
 * @param[in] length Number of bytes to write; no string terminator is required.
 * @note Uses stdio and may block; never call inside an edge-generation loop or ISR.
 */
void debug_serial_send_stdout(const uint8_t *data, size_t length)
{
    if (data != NULL)
    {
        (void)fwrite(data, 1U, length, stdout);
        (void)fflush(stdout);
    }
}
/**
 * @brief Reject the unsupported BMD target-side SPI transport.
 * @param[in] bus Ignored backend bus selection.
 * @return false; this hook is unrelated to the separate microSD SPI component.
 */
bool platform_spi_init(spi_bus_e bus)
{
    (void)bus;
    return false;
}
/**
 * @brief Report that the BMD target-side SPI transport is unavailable.
 * @param[in] bus Ignored backend bus selection.
 * @return false; no hardware state is changed.
 */
bool platform_spi_deinit(spi_bus_e bus)
{
    (void)bus;
    return false;
}
/**
 * @brief Reject target-side SPI chip selection without touching GPIO.
 * @param[in] select Ignored backend chip-select identifier.
 * @return false because the transport is unavailable.
 */
bool platform_spi_chip_select(uint8_t select)
{
    (void)select;
    return false;
}
/**
 * @brief Return an idle value for the unsupported BMD SPI transfer hook.
 * @param[in] bus Ignored backend bus selection.
 * @param[in] value Ignored output byte.
 * @return UINT8_MAX; no byte is transmitted.
 */
uint8_t platform_spi_xfer(spi_bus_e bus, uint8_t value)
{
    (void)bus;
    (void)value;
    return UINT8_MAX;
}
