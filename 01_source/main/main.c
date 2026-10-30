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
#include "nvs_flash.h"
#include "probe_network.h"
#include "probe_debug.h"
#include "probe_console.h"
#include "probe_local_ui.h"

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
    esp_err_t storage = nvs_flash_init();
    if ((storage == ESP_ERR_NVS_NO_FREE_PAGES) || (storage == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        storage = nvs_flash_init();
    }
    ESP_ERROR_CHECK(storage);
    ESP_ERROR_CHECK(probe_network_start());
    ESP_ERROR_CHECK(probe_debug_start());
    ESP_ERROR_CHECK(probe_local_ui_start());
    ESP_ERROR_CHECK(probe_console_start());
    ESP_LOGI("probe_app", "Probe services started; use 'status' or 'help' on serial");
}
