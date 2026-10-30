/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_board.h
 * @brief Startup validation of the ESP32-S3 N16R8 hardware profile.
 */

#ifndef PROBE_BOARD_H
#define PROBE_BOARD_H

#include "esp_err.h"

/**
 * @brief Validate the chip, flash geometry and initialized PSRAM.
 *
 * @pre ESP-IDF startup has initialized flash and PSRAM.
 * @note Call once from the application startup task before starting probe services.
 *       The function reads hardware information and emits diagnostic logs.
 * @return ESP_OK when the dual-core S3, 16 MiB flash and 8 MiB PSRAM match the profile.
 * @retval ESP_ERR_NOT_SUPPORTED The chip or core count does not match the profile.
 * @retval ESP_ERR_INVALID_SIZE The physical/configured flash or PSRAM size is incorrect.
 * @retval ESP_ERR_INVALID_STATE PSRAM has not been initialized.
 * @retval ESP_ERR_INVALID_ARG Pin assignments overlap or use a reserved board pin.
 * @note Flash driver errors are returned unchanged. The function does not abort or reset.
 */
esp_err_t probe_board_check(void);

#endif /* PROBE_BOARD_H */
