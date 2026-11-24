/*
 * SPDX-FileCopyrightText: 2026 Piotr Wasilewski
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * C adaptation of MCUViewer-probe Platform/SwdTap.cpp, commit
 * 02755f409634f0874fb087b720d8883ef6f65dd8. Only the core-1 owner calls this port.
 */
/**
 * @file probe_swd.c
 * @brief Cycle-counted dedicated-GPIO SWD port.
 *
 * All calls belong to the core-1 debug owner. Tight edge-generation loops use IRAM and cycle
 * counters; they perform no allocation, logging or task synchronization.
 */
#include "probe_port.h"
#include <inttypes.h>
#include <stddef.h>
#include "driver/dedic_gpio.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
/* SDK-005: register bitfield conversions belong to upstream LL inline functions. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "hal/dedic_gpio_cpu_ll.h"
#include "hal/gpio_ll.h"
#pragma GCC diagnostic pop
#include "sdkconfig.h"
#include "soc/gpio_struct.h"
#include "general.h"
#include "swd.h"
static uint32_t clock_mask;
static uint32_t data_mask;
static uint32_t input_mask;
static uint32_t cpu_hz;
static uint32_t half_period;
static uint32_t fastest_hz;
static bool data_driven;
static dedic_gpio_bundle_handle_t gpio_bundle;
swd_proc_s swd_proc;
/**
 * @brief Wait for a modular CPU-cycle deadline without compressing delayed pulses.
 * @param[in] edge Previous edge timestamp in CPU cycles.
 * @param[in] half Requested half-period in CPU cycles; zero uses the fastest loop.
 * @return Actual or scheduled edge timestamp for the next half-period.
 * @pre Deadlines span less than half the 32-bit cycle-counter range.
 */
static inline __attribute__((always_inline)) uint32_t swd_wait(uint32_t edge, uint32_t half)
{
    uint32_t now;
    edge += half;
    do
    {
        now = esp_cpu_get_cycle_count();
    } while ((int32_t)(now - edge) < 0);
    if ((now - edge) > (half >> 1U))
    {
        edge = now;
    }
    return edge;
}
/**
 * @brief Shift output bits LSB-first with low/high SWCLK half-periods.
 * @param[in] value Low bits to transmit; zero bits follow after the value is exhausted.
 * @param[in] cycles Number of clock cycles.
 * @param[in] half Half-period in CPU cycles.
 * @pre The core-1 owner has configured and enabled the dedicated GPIO bundle.
 */
static IRAM_ATTR void swd_output(uint32_t value, size_t cycles, uint32_t half)
{
    uint32_t edge = esp_cpu_get_cycle_count();
    for (size_t bit = 0U; bit < cycles; bit++)
    {
        dedic_gpio_cpu_ll_write_mask(clock_mask | data_mask, ((value & 1U) != 0U) ? data_mask : 0U);
        value >>= 1U;
        edge = swd_wait(edge, half);
        dedic_gpio_cpu_ll_write_mask(clock_mask, clock_mask);
        edge = swd_wait(edge, half);
    }
    dedic_gpio_cpu_ll_write_mask(clock_mask, 0U);
}
/**
 * @brief Sample SWDIO and assemble at most 32 bits in LSB-first order.
 * @param[in] cycles Number of input bits, at most 32.
 * @param[in] half Half-period in CPU cycles.
 * @return Sampled bits with the first bit in bit zero.
 * @pre The core-1 owner has released SWDIO for target-driven input.
 */
static IRAM_ATTR uint32_t swd_input(size_t cycles, uint32_t half)
{
    uint32_t edge = esp_cpu_get_cycle_count();
    uint32_t value = 0U;
    for (size_t bit = 0U; bit < cycles; bit++)
    {
        edge = swd_wait(edge, half);
        const uint32_t data = ((dedic_gpio_cpu_ll_read_in() & input_mask) != 0U) ? 1U : 0U;
        dedic_gpio_cpu_ll_write_mask(clock_mask, clock_mask);
        value |= data << bit;
        edge = swd_wait(edge, half);
        dedic_gpio_cpu_ll_write_mask(clock_mask, 0U);
    }
    return value;
}
/**
 * @brief Insert a turnaround clock only when SWDIO ownership changes.
 * @param[in] drive True for probe-driven output; false for target-driven input.
 * @param[in] half Half-period in CPU cycles.
 * @note Releases the output before input turnaround and enables it after output turnaround.
 */
static IRAM_ATTR void swd_turnaround(bool drive, uint32_t half)
{
    if (drive != data_driven)
    {
        data_driven = drive;
        if (!drive)
        {
            GPIO.enable_w1tc = UINT32_C(1) << CONFIG_PROBE_SWDIO_GPIO;
        }
        uint32_t edge = esp_cpu_get_cycle_count();
        edge = swd_wait(edge, half);
        dedic_gpio_cpu_ll_write_mask(clock_mask, clock_mask);
        (void)swd_wait(edge, half);
        dedic_gpio_cpu_ll_write_mask(clock_mask, 0U);
        if (drive)
        {
            GPIO.enable_w1ts = UINT32_C(1) << CONFIG_PROBE_SWDIO_GPIO;
        }
    }
}
/**
 * @brief Release SWDIO and read a backend-requested bit sequence using the selected clock.
 */
static IRAM_ATTR uint32_t swd_read(size_t cycles)
{
    swd_turnaround(false, half_period);
    return swd_input(cycles, half_period);
}
/**
 * @brief Read data and an even-parity bit, then return SWDIO to output mode.
 * @param[out] value Non-NULL destination for the sampled word.
 * @param[in] cycles Number of data bits, at most 32.
 * @return true when received parity matches the sampled data.
 */
static IRAM_ATTR bool swd_read_parity(uint32_t *value, size_t cycles)
{
    swd_turnaround(false, half_period);
    const uint32_t data = swd_input(cycles, half_period);
    const uint32_t parity = swd_input(1U, half_period);
    swd_turnaround(true, half_period);
    *value = data;
    return (uint32_t)__builtin_parity(data) == parity;
}
/**
 * @brief Acquire SWDIO and transmit the requested LSB-first bit sequence.
 */
static IRAM_ATTR void swd_write(uint32_t value, size_t cycles)
{
    swd_turnaround(true, half_period);
    swd_output(value, cycles, half_period);
}
/**
 * @brief Transmit the word followed by its computed even-parity bit.
 */
static IRAM_ATTR void swd_write_parity(uint32_t value, size_t cycles)
{
    swd_turnaround(true, half_period);
    swd_output(value, cycles, half_period);
    swd_output((uint32_t)__builtin_parity(value), 1U, half_period);
}
/**
 * @brief Bind the BMD SWD procedure table to this port without allocating resources.
 */
void swdptap_init(void)
{
    swd_proc.seq_in = swd_read;
    swd_proc.seq_in_parity = swd_read_parity;
    swd_proc.seq_out = swd_write;
    swd_proc.seq_out_parity = swd_write_parity;
}
/**
 * @brief Enable probe SWD outputs or release both pins without changing their pull-ups.
 */
void probe_swd_drive(bool enable)
{
    const uint32_t pins =
        (UINT32_C(1) << CONFIG_PROBE_SWCLK_GPIO) | (UINT32_C(1) << CONFIG_PROBE_SWDIO_GPIO);
    if (enable)
    {
        dedic_gpio_cpu_ll_write_mask(clock_mask | data_mask, data_mask);
        GPIO.enable_w1ts = pins;
    }
    else
    {
        GPIO.enable_w1tc = pins;
    }
    data_driven = enable;
}
/**
 * @brief Clamp the requested clock and derive the cycle-counted half-period.
 */
void probe_swd_set_frequency(uint32_t hz)
{
    uint32_t requested = hz;
    if (requested < 1000U)
    {
        requested = 1000U;
    }
    if (requested > 8000000U)
    {
        requested = 8000000U;
    }
    half_period =
        (requested >= fastest_hz) ? 0U : (cpu_hz + (2U * requested) - 1U) / (2U * requested);
}
/**
 * @brief Return the selected nominal clock limited by startup calibration.
 */
uint32_t probe_swd_get_frequency(void)
{
    uint32_t hz = fastest_hz;
    if (half_period != 0U)
    {
        hz = cpu_hz / (2U * half_period);
        if (hz > fastest_hz)
        {
            hz = fastest_hz;
        }
    }
    return hz;
}
/**
 * @brief Bind dedicated GPIO on the owner core and calibrate its fastest output loop.
 * @return ESP_OK on success or the first GPIO setup error.
 * @pre Called once by the core-1 debug owner before any backend operation.
 * @note Calibration runs with GPIO outputs released; it does not intentionally clock a target.
 */
esp_err_t probe_port_init(void)
{
    const int pins[] = {CONFIG_PROBE_SWCLK_GPIO, CONFIG_PROBE_SWDIO_GPIO};
    dedic_gpio_bundle_config_t config = {
        .gpio_array = pins, .array_size = 2U, .flags = {.in_en = 1U, .out_en = 1U}};
    esp_err_t result = dedic_gpio_new_bundle(&config, &gpio_bundle);
    uint32_t output_offset = 0U;
    uint32_t input_offset = 0U;
    if (result == ESP_OK)
    {
        result = dedic_gpio_get_out_offset(gpio_bundle, &output_offset);
    }
    if (result == ESP_OK)
    {
        result = dedic_gpio_get_in_offset(gpio_bundle, &input_offset);
    }
    if (result == ESP_OK)
    {
        clock_mask = UINT32_C(1) << output_offset;
        data_mask = UINT32_C(1) << (output_offset + 1U);
        input_mask = UINT32_C(1) << (input_offset + 1U);
        gpio_ll_set_output_enable_ctrl(&GPIO, CONFIG_PROBE_SWCLK_GPIO, false, false);
        gpio_ll_set_output_enable_ctrl(&GPIO, CONFIG_PROBE_SWDIO_GPIO, false, false);
        result = gpio_pullup_en((gpio_num_t)CONFIG_PROBE_SWDIO_GPIO);
        probe_swd_drive(false);
        cpu_hz = esp_rom_get_cpu_ticks_per_us() * 1000000U;
        const uint32_t start = esp_cpu_get_cycle_count();
        swd_output(UINT32_C(0x55555555), 256U, 0U);
        const uint32_t elapsed = esp_cpu_get_cycle_count() - start;
        fastest_hz = (elapsed == 0U) ? 1000U : (uint32_t)(((uint64_t)256U * cpu_hz) / elapsed);
        probe_swd_set_frequency(CONFIG_PROBE_SWD_DEFAULT_HZ);
        swdptap_init();
        ESP_LOGI("probe_swd", "Core %d SWDIO=%d SWCLK=%d nominal=%" PRIu32 " Hz",
                 esp_cpu_get_core_id(), CONFIG_PROBE_SWDIO_GPIO, CONFIG_PROBE_SWCLK_GPIO,
                 probe_swd_get_frequency());
    }
    return result;
}
