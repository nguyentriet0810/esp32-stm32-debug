/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file main.c
 * @brief Application startup and board validation policy.
 */

#include "probe_board.h"

#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_log.h"

/* ESP-IDF invokes this externally visible entry point from its main task. */
void app_main(void);

/** @brief Validate the board before any probe service can start. */
void app_main(void)
{
    const esp_err_t result = probe_board_check();

    /* SDK-002: startup failure is fatal; components return errors to this policy layer. */
    ESP_ERROR_CHECK(result);

    /* SDK-001: bounded startup diagnostics through the ESP-IDF logging interface. */
    ESP_LOGI("probe_app", "ESP-IDF %s", esp_get_idf_version());
    ESP_LOGI("probe_app", "Board bring-up passed: ESP32-S3 N16R8");
}
