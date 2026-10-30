/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_debug.h
 * @brief Single-owner GDB/SWD service. Public calls are task-context only.
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
 * @brief Queue a new clock; the owner applies it between sessions.
 * @param[in] hz Requested frequency, 1000..8000000 Hz, clamped to the calibrated port limit.
 * @return ESP_OK if queued, INVALID_STATE while busy, INVALID_ARG out of range,
 *         TIMEOUT if the command queue is full. Task context only; no persistence.
 */
esp_err_t probe_debug_set_clock(uint32_t hz);
/**
 * @brief Reserve idle state for NVS/network configuration from task context.
 * @return true if acquired; false when another configuration or GDB session owns the probe.
 * @note A successful acquisition must be paired with unlock even if configuration fails.
 */
bool probe_debug_configuration_lock(void);
/** @brief Release a configuration reservation held by the calling task. */
void probe_debug_configuration_unlock(void);
#endif
