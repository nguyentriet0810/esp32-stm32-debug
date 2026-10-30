/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_local_ui.h @brief Nonfatal OLED/encoder application component. */
#ifndef PROBE_LOCAL_UI_H
#define PROBE_LOCAL_UI_H
#include "esp_err.h"
/**
 * @brief Start the menu and optional input tasks after the debug service.
 * @pre Call once from startup task. Missing OLED is reported asynchronously, nonfatal.
 * @return ESP_OK, allocation error, or encoder GPIO initialization error.
 */
esp_err_t probe_local_ui_start(void);
#endif
