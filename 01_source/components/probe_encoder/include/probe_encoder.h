/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_encoder.h
 * @brief Queued encoder and button input API.
 *
 * The sampling task debounces the physical inputs and sends bounded events to the UI queue
 * without accessing SWD or I2C.
 */
#ifndef PROBE_ENCODER_H
#define PROBE_ENCODER_H
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
/** @brief Copied UI event; each rotation event represents one complete detent. */
typedef enum
{
    PROBE_ENCODER_LEFT,  /**< Negative full-detent movement. */
    PROBE_ENCODER_RIGHT, /**< Positive full-detent movement. */
    PROBE_ENCODER_CLICK, /**< Debounced button release after a short press. */
    PROBE_ENCODER_BACK   /**< One long-press event; suppresses the release click. */
} probe_encoder_event_t;
/**
 * @brief Configure encoder inputs and start the 1 ms sampling task.
 * @param[in] events Queue of probe_encoder_event_t, owned by the caller for task lifetime.
 * @retval ESP_OK GPIO setup and task creation succeeded.
 * @retval ESP_ERR_INVALID_ARG The queue is NULL when input support is enabled.
 * @retval ESP_ERR_NOT_SUPPORTED Encoder support is disabled in the build.
 * @return GPIO errors or ESP_ERR_NO_MEM are returned on initialization failure.
 * @pre Call once from startup; the queue must exist before this call.
 * @note Task context only. Events are copied; a full queue drops the new event.
 */
esp_err_t probe_encoder_start(QueueHandle_t events);
#endif
