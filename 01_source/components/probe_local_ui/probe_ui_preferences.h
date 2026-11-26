/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file probe_ui_preferences.h @brief Private NVS access for committed local UI preferences. */
#ifndef PROBE_UI_PREFERENCES_H
#define PROBE_UI_PREFERENCES_H
#include <stdint.h>
#include "esp_err.h"
/**
 * @brief Read the stored OLED percentage without altering NVS or applying display changes.
 * @param[out] percent Non-NULL output; written only on success, validated by the UI model.
 * @return ESP_OK, ESP_ERR_NVS_NOT_FOUND for an unset preference, or an SDK read error.
 * @pre NVS is initialized. Called by the display-owning task before menu interaction.
 */
esp_err_t probe_ui_preferences_oled_load(uint8_t *percent);
/**
 * @brief Commit a changed OLED percentage; an identical stored value performs no write.
 * @param[in] percent Committed UI percentage, one of 10/25/50/75/100.
 * @return ESP_OK on a successful commit/no change, or the first SDK storage error.
 * @pre NVS is initialized and the caller holds the debug configuration reservation.
 * @note UI task context only; never call for preview rotation or cancellation. No other
 *       namespace/key is changed or erased. Every opened handle is closed before return.
 */
esp_err_t probe_ui_preferences_oled_save(uint8_t percent);
#endif
