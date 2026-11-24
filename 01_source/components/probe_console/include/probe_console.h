/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_console.h
 * @brief Bounded serial command console API.
 *
 * Consumes ESP-IDF stdin/stdout, so the selected UART or USB console is shared with diagnostics.
 * Commands run in the console task; input is not echoed.
 */
#ifndef PROBE_CONSOLE_H
#define PROBE_CONSOLE_H
#include "esp_err.h"
/**
 * @brief Start the bounded diagnostic and provisioning console task.
 * @pre Call once from startup after NVS, network and debug services are ready.
 * @retval ESP_OK The task was created.
 * @retval ESP_ERR_NO_MEM FreeRTOS could not allocate the task.
 * @note Uses ESP-IDF stdin/stdout on the configured UART or native USB Serial/JTAG console.
 *       Input is not echoed; commands and diagnostic logs share that console.
 */
esp_err_t probe_console_start(void);
#endif
