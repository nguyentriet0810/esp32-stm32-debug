/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_port.h
 * @brief Private GPIO ownership and SWD timing interface.
 *
 * Initialize and call this port only from the core-1 debug owner. Other components request
 * changes through probe_debug.h.
 */
#ifndef PROBE_PORT_H
#define PROBE_PORT_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
/**
 * @brief Initialize dedicated GPIO and calibrate nominal SWD timing.
 * @return ESP_OK or the first GPIO setup error.
 * @pre Call once from the core-1 owner before all other port APIs.
 * @note Handles live for the owner-task lifetime; this port has no public teardown API.
 */
esp_err_t probe_port_init(void);
/**
 * @brief Select a nominal clock without exceeding the calibrated port limit.
 * @param[in] hz Requested frequency in Hz, clamped to 1000..8000000.
 * @pre The core-1 owner initialized the port and is between target transactions.
 * @note Purely updates timing state; no NVS write or cross-task synchronization occurs.
 */
void probe_swd_set_frequency(uint32_t hz);
/**
 * @brief Read the selected nominal SWD clock.
 * @return Frequency in Hz, limited by startup calibration.
 * @pre Call from the core-1 owner after successful port initialization.
 */
uint32_t probe_swd_get_frequency(void);
/**
 * @brief Enable probe-driven SWD outputs or release both pins.
 * @param[in] enable True to enable output; false to make SWCLK/SWDIO inputs.
 * @pre The calling core-1 owner successfully initialized the dedicated GPIO bundle.
 * @note The enabled idle levels are SWCLK low and SWDIO high.
 */
void probe_swd_drive(bool enable);
#endif
