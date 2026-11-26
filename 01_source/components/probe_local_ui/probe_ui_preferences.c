/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file probe_ui_preferences.c @brief Store committed OLED level separately from Wi-Fi data. */
#include "probe_ui_preferences.h"
#include <stddef.h>
#include "nvs.h"

static const char preferences_namespace[] = "probe_ui";
static const char oled_key[] = "oled_pct";

/** @brief Read one typed byte and leave the caller's output untouched on every error. */
esp_err_t probe_ui_preferences_oled_load(uint8_t *percent)
{
    if (percent == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t handle = 0U;
    esp_err_t result = nvs_open(preferences_namespace, NVS_READONLY, &handle);
    if (result == ESP_OK)
    {
        uint8_t value = 0U;
        result = nvs_get_u8(handle, oled_key, &value);
        nvs_close(handle);
        if (result == ESP_OK)
        {
            *percent = value;
        }
    }
    return result;
}

/** @brief Compare before writing so repeated saves do not consume flash cycles. */
esp_err_t probe_ui_preferences_oled_save(uint8_t percent)
{
    nvs_handle_t handle = 0U;
    esp_err_t result = nvs_open(preferences_namespace, NVS_READWRITE, &handle);
    if (result == ESP_OK)
    {
        uint8_t previous = 0U;
        result = nvs_get_u8(handle, oled_key, &previous);
        if ((result == ESP_ERR_NVS_NOT_FOUND) || (result == ESP_ERR_NVS_TYPE_MISMATCH) ||
            ((result == ESP_OK) && (previous != percent)))
        {
            result = nvs_set_u8(handle, oled_key, percent);
            if (result == ESP_OK)
            {
                result = nvs_commit(handle);
            }
        }
        nvs_close(handle);
    }
    return result;
}
