/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_board.c
 * @brief ESP-IDF adapter for validating the selected N16R8 board profile.
 */

#include "probe_board.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"

static const char probe_board_log_tag[] = "probe_board";

/** @brief Read both flash sizes before comparing them with the board profile. */
static esp_err_t probe_board_check_flash(void)
{
    const uint32_t expected_flash_bytes = UINT32_C(16777216); /* 16 MiB. */
    uint32_t physical_bytes = UINT32_C(0);
    uint32_t configured_bytes = UINT32_C(0);
    esp_err_t result = esp_flash_get_physical_size(NULL, &physical_bytes);

    if (result == ESP_OK)
    {
        result = esp_flash_get_size(NULL, &configured_bytes);
    }

    if (result == ESP_OK)
    {
        /* SDK-001: ESP-IDF logging macros are confined to task context. */
        ESP_LOGI(probe_board_log_tag,
                 "Flash: physical %" PRIu32 " bytes, configured %" PRIu32 " bytes",
                 physical_bytes, configured_bytes);
        if ((physical_bytes != expected_flash_bytes) ||
            (configured_bytes != physical_bytes))
        {
            ESP_LOGE(probe_board_log_tag, "Expected 16 MiB physical and configured flash");
            result = ESP_ERR_INVALID_SIZE;
        }
    }

    return result;
}

/** @brief Check readiness before asking the SDK for the attached PSRAM capacity. */
static esp_err_t probe_board_check_psram(void)
{
    const size_t expected_psram_bytes = (size_t)8388608U; /* 8 MiB. */
    esp_err_t result = ESP_OK;

    if (esp_psram_is_initialized())
    {
        const size_t psram_bytes = esp_psram_get_size();

        ESP_LOGI(probe_board_log_tag, "PSRAM: %zu bytes", psram_bytes);
        if (psram_bytes != expected_psram_bytes)
        {
            ESP_LOGE(probe_board_log_tag, "Expected 8 MiB Octal PSRAM");
            result = ESP_ERR_INVALID_SIZE;
        }
    }
    else
    {
        ESP_LOGE(probe_board_log_tag, "PSRAM is not initialized");
        result = ESP_ERR_INVALID_STATE;
    }

    return result;
}

esp_err_t probe_board_check(void)
{
    const uint8_t expected_core_count = (uint8_t)2U;
    esp_chip_info_t chip_info = {0};
    esp_err_t result = ESP_OK;

    esp_chip_info(&chip_info);
    ESP_LOGI(probe_board_log_tag, "Chip revision %" PRIu32 "; cores %" PRIu32,
             (uint32_t)chip_info.revision, (uint32_t)chip_info.cores);

    if ((chip_info.model != CHIP_ESP32S3) || (chip_info.cores != expected_core_count))
    {
        ESP_LOGE(probe_board_log_tag, "Expected a dual-core ESP32-S3");
        result = ESP_ERR_NOT_SUPPORTED;
    }
    else
    {
        result = probe_board_check_flash();
    }

    if (result == ESP_OK)
    {
        result = probe_board_check_psram();
    }

    if (result == ESP_OK)
    {
        ESP_LOGI(probe_board_log_tag, "Free internal heap: %zu bytes",
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        ESP_LOGI(probe_board_log_tag, "Free PSRAM heap: %zu bytes",
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }

    return result;
}
