/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_encoder.h @brief Optional quadrature/button input and testable decoder. */
#ifndef PROBE_ENCODER_H
#define PROBE_ENCODER_H
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
typedef enum
{
    PROBE_ENCODER_LEFT,
    PROBE_ENCODER_RIGHT,
    PROBE_ENCODER_CLICK,
    PROBE_ENCODER_BACK
} probe_encoder_event_t;
/**
 * @brief Start a 1 ms input task, once during startup.
 * @param[in] events Queue of probe_encoder_event_t, owned by caller and kept for task lifetime.
 * @return ESP_OK, INVALID_ARG for NULL queue, NOT_SUPPORTED if disabled, or GPIO/allocation error.
 * @note Task context only. A full queue drops the new event and reports overflow.
 */
esp_err_t probe_encoder_start(QueueHandle_t events);
#endif
