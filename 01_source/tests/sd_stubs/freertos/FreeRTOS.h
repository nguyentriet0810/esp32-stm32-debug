/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file FreeRTOS.h
 * @brief FreeRTOS scalar and timeout subset for host tests.
 *
 * Tick conversions are deterministic test values, not a scheduler or timing simulation. Test-
 * only header; not used in firmware builds.
 */
#ifndef SD_TEST_FREERTOS_H
#define SD_TEST_FREERTOS_H
#include <stdint.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(milliseconds) ((TickType_t)(milliseconds))
#endif
