/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_debug.h
 * @brief Single-owner GDB and SWD service API.
 *
 * The core-0 listener transfers accepted sockets to a core-1 owner. Only that owner touches the
 * Black Magic Debug backend and the target GPIO.
 */
#ifndef PROBE_DEBUG_H
#define PROBE_DEBUG_H
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
/**
 * @brief Initialize the core-1 SWD owner before starting the TCP listener.
 * @pre Call once after network initialization, in task context.
 * @return ESP_OK when the owner initialized; allocation, GPIO or startup timeout error otherwise.
 * @note TCP bind/listen readiness is reported asynchronously through the log.
 */
esp_err_t probe_debug_start(void);
/**
 * @brief Queue a nominal SWD clock for the owner to apply between sessions.
 * @param[in] hz Requested frequency, 1000..8000000 Hz; the port also clamps to its limit.
 * @retval ESP_OK The request was queued, not necessarily applied before return.
 * @retval ESP_ERR_INVALID_STATE A configuration reservation or GDB session is active.
 * @retval ESP_ERR_INVALID_ARG The clock is out of range or the command queue is unavailable.
 * @retval ESP_ERR_TIMEOUT The nonblocking queue send failed.
 * @pre probe_debug_start has completed. Call from task context.
 * @note Does not persist the selection in NVS.
 */
esp_err_t probe_debug_set_clock(uint32_t hz);
/**
 * @brief Reserve idle state for NVS/network configuration from task context.
 * @return true if acquired; false when another configuration or GDB session owns the probe.
 * @note A successful acquisition must be paired with unlock even if configuration fails.
 */
bool probe_debug_configuration_lock(void);
/**
 * @brief Release the idle-state reservation acquired for configuration.
 * @pre The calling task successfully acquired probe_debug_configuration_lock.
 * @note Task context only. Pair every successful lock with exactly one unlock, including
 *       configuration error paths. The reservation is not a recursive mutex.
 */
void probe_debug_configuration_unlock(void);
#endif
