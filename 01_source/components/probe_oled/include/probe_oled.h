/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_oled.h @brief Optional SSD1306 text display, single UI-task ownership. */
#ifndef PROBE_OLED_H
#define PROBE_OLED_H
#include <stddef.h>
#include "esp_err.h"
/**
 * @brief Detect 0x3C/0x3D and initialize the display, once in the UI task.
 * @return ESP_OK, NOT_FOUND on missing/failed display, or NOT_SUPPORTED if disabled.
 * @note One UI task owns all OLED APIs; not reentrant or callable from ISR.
 */
esp_err_t probe_oled_init(void);
/**
 * @brief Render ASCII lines into a fixed framebuffer and transfer them over I2C.
 * @param[in] lines Caller-owned rows, each up to 21 characters plus terminator.
 * @param[in] count Number of rows, 0..8. Data is consumed before return.
 * @return ESP_OK, INVALID_STATE if uninitialized/invalid input, or I2C transfer error.
 */
esp_err_t probe_oled_draw(const char lines[][22], size_t count);
/**
 * @brief Change contrast from the same task that renders the display.
 * @param[in] value Contrast, 0..255.
 * @return ESP_OK, INVALID_ARG if uninitialized/out of range, or I2C error.
 */
esp_err_t probe_oled_contrast(unsigned int value);
#endif
