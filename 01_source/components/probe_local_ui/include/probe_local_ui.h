/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_local_ui.h
 * @brief Local OLED and encoder menu API.
 *
 * The UI task owns display operations and requests clock changes through the debug service.
 * Optional display failures are reported without stopping the debug service.
 */
#ifndef PROBE_LOCAL_UI_H
#define PROBE_LOCAL_UI_H
#include "esp_err.h"
/**
 * @brief Create the menu queue and start optional input and display-owning tasks.
 * @pre Call once from the startup task after debug-service initialization.
 * @retval ESP_OK The UI task was created; display detection is still asynchronous.
 * @return ESP_ERR_NO_MEM or an encoder GPIO initialization error on failure.
 * @note An absent/failed OLED and a disabled encoder are nonfatal. Queue and task
 *       resources are intended to live for the firmware lifetime.
 */
esp_err_t probe_local_ui_start(void);
#endif
