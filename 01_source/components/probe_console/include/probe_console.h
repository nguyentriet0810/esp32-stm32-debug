/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_console.h @brief Bounded UART console; credential input is never echoed. */
#ifndef PROBE_CONSOLE_H
#define PROBE_CONSOLE_H
#include "esp_err.h"
/**
 * @brief Start the UART0 diagnostic/provisioning task after services are ready.
 * @pre Call once from startup task; ESP-IDF stdin/stdout must be configured.
 * @return ESP_OK or ESP_ERR_NO_MEM if task allocation fails.
 */
esp_err_t probe_console_start(void);
#endif
