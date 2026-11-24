/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_oled.h
 * @brief SSD1306 display lifecycle and text rendering API.
 *
 * A single UI task owns the I2C bus, panel handles and fixed framebuffer. Driver errors are
 * returned to the caller.
 */
#ifndef PROBE_OLED_H
#define PROBE_OLED_H
#include <stddef.h>
#include "esp_err.h"
/**
 * @brief Probe addresses 0x3C/0x3D and initialize the optional SSD1306 display.
 * @retval ESP_OK The panel is ready for rendering.
 * @retval ESP_ERR_NOT_FOUND An enabled display could not be initialized.
 * @retval ESP_ERR_NOT_SUPPORTED OLED support is disabled in this build.
 * @pre Call once from the UI task before drawing or changing contrast.
 * @note The same task owns all OLED APIs. They are not reentrant or ISR-safe.
 *       A failed enabled initialization releases the acquired driver handles.
 */
esp_err_t probe_oled_init(void);
/**
 * @brief Render supported ASCII glyphs and transfer the fixed 128x64 framebuffer.
 * @param[in] lines Borrowed rows of 22 bytes; at most 21 characters per row are consumed.
 * @param[in] count Number of rows, 0..8. Input is consumed before return.
 * @retval ESP_OK The panel driver accepted the frame transfer.
 * @retval ESP_ERR_INVALID_STATE The panel/input is unavailable or count exceeds eight.
 * @return I2C transfer errors are returned unchanged.
 * @pre probe_oled_init succeeded and the display-owning UI task is the caller.
 * @note Lowercase glyphs are rendered as uppercase; unsupported glyphs remain blank.
 */
esp_err_t probe_oled_draw(const char lines[][22], size_t count);
/**
 * @brief Set the SSD1306 contrast from the display-owning task.
 * @param[in] value Unsigned contrast setting, 0..255.
 * @retval ESP_OK The contrast command was transferred.
 * @retval ESP_ERR_INVALID_ARG The panel I/O is unavailable or value exceeds 255.
 * @return I2C transfer errors are returned unchanged.
 * @pre probe_oled_init succeeded. Not reentrant or callable from ISR.
 */
esp_err_t probe_oled_contrast(unsigned int value);
#endif
