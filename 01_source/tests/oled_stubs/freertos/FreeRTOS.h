/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file FreeRTOS.h @brief Deterministic millisecond ticks for OLED host tests. */
#ifndef OLED_TEST_FREERTOS_H
#define OLED_TEST_FREERTOS_H
#include <stdint.h>
typedef uint32_t TickType_t;
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#endif
