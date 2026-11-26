/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file nvs.h @brief Test-only NVS vocabulary for local OLED preference fault injection. */
#ifndef OLED_TEST_NVS_H
#define OLED_TEST_NVS_H
#include <stdint.h>
#include "esp_err.h"
typedef uint32_t nvs_handle_t;
typedef enum
{
    NVS_READONLY,
    NVS_READWRITE
} nvs_open_mode_t;
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define ESP_ERR_NVS_TYPE_MISMATCH 0x1103
esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *handle);
esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *value);
esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value);
esp_err_t nvs_commit(nvs_handle_t handle);
void nvs_close(nvs_handle_t handle);
#endif
