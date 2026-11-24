/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file esp_err.h
 * @brief ESP-IDF error-code subset for host SD tests.
 *
 * Values are test-local stand-ins; they are not an ESP-IDF ABI compatibility promise. Test-only
 * header; not used in firmware builds.
 */
#ifndef SD_TEST_ESP_ERR_H
#define SD_TEST_ESP_ERR_H
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL (-1)
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
/**
 * @brief Return a fixed diagnostic label; numeric error identity is asserted separately.
 */
const char *esp_err_to_name(esp_err_t error);
#endif
