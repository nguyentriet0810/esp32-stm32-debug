/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_oled.h
 * @brief SH1106 128x64 display lifecycle and text rendering API.
 *
 * A single UI task owns the I2C bus, panel I/O and fixed framebuffer. Driver errors are
 * returned to the caller.
 */
#ifndef PROBE_OLED_H
#define PROBE_OLED_H
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define PROBE_OLED_TEXT_ROWS 8U
#define PROBE_OLED_TEXT_COLUMNS 16U

/** @brief Diagonal startup stages, expressed in the current readable display orientation. */
typedef enum
{
    PROBE_OLED_STARTUP_FILL,  /**< Black to white, from the upper-left to the lower-right. */
    PROBE_OLED_STARTUP_REVEAL /**< White to the three-line wordmark using the same direction. */
} probe_oled_startup_phase_t;
/**
 * @brief Probe addresses 0x3C/0x3D and initialize the optional SH1106 display.
 * @retval ESP_OK The panel is ready for rendering.
 * @retval ESP_ERR_NOT_FOUND An enabled display could not be initialized.
 * @retval ESP_ERR_NOT_SUPPORTED OLED support is disabled in this build.
 * @retval ESP_ERR_INVALID_STATE This component already owns an initialized I2C bus or I/O.
 * @pre Call once from the UI task before drawing or changing contrast.
 * @note The same task owns all OLED APIs. They are not reentrant or ISR-safe.
 *       A failed enabled initialization releases the acquired driver handles.
 *       Segment/common scan directions rotate the image 180 degrees from A1/C8 mapping.
 */
esp_err_t probe_oled_init(void);
/**
 * @brief Render sixteen 8x8 character cells per row into the fixed 128x64 framebuffer.
 * @param[in] lines Borrowed rows of 17 bytes; at most 16 characters per row are consumed.
 * @param[in] count Number of rows, 0..8. Input is consumed before return.
 * @retval ESP_OK The panel driver accepted the frame transfer.
 * @retval ESP_ERR_INVALID_STATE The panel/input is unavailable or count exceeds eight.
 * @return I2C transfer errors are returned unchanged.
 * @pre probe_oled_init succeeded and the display-owning UI task is the caller.
 * @note Uses the owner's font8x8_basic_tr: eight columns, bit 0 at the top, case preserved.
 *       Codes 0x01/0x02 draw up/down arrows; bytes above 0x7F remain blank. All eight pixel
 *       rows are significant, including descenders and underscores on the last row.
 *       Sends eight explicitly addressed pages. CONFIG_PROBE_OLED_COLUMN_OFFSET selects
 *       the visible 128 columns within the controller's 132-column RAM; default is 2.
 */
esp_err_t probe_oled_draw(const char lines[][PROBE_OLED_TEXT_COLUMNS + 1U], size_t count);
/**
 * @brief Draw one diagonal startup frame and update the controller's brightness/display state.
 * @param[in] phase Fill the screen or erase its background while retaining MAGIC/DEBUGGER/TOOL.
 * @param[in] progress Stage progress, 0..255, inclusive; endpoints are exact images.
 * @retval ESP_OK The complete frame was transferred.
 * @retval ESP_ERR_INVALID_STATE The OLED is unavailable or phase is not supported.
 * @return I2C command/data errors are returned unchanged; transmission stops on first error.
 * @pre probe_oled_init succeeded; only the display-owning UI task may call this API.
 * @note FILL at zero turns the panel off and stores black. Other frames turn it on; the fill
 *       contrast rises to 128. REVEAL ends with centered two-times-size text on black.
 *       Coordinates match normal text after the configured 180-degree controller rotation.
 */
esp_err_t probe_oled_startup_frame(probe_oled_startup_phase_t phase, uint8_t progress);
/**
 * @brief Set the SH1106 contrast from the display-owning task.
 * @param[in] value Unsigned contrast setting, 0..255.
 * @retval ESP_OK The contrast command was transferred.
 * @retval ESP_ERR_INVALID_ARG The panel I/O is unavailable or value exceeds 255.
 * @return I2C transfer errors are returned unchanged.
 * @pre probe_oled_init succeeded. Not reentrant or callable from ISR.
 */
esp_err_t probe_oled_contrast(unsigned int value);
#endif
