/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file task.h @brief Startup-delay observation hook; no real scheduler runs in host tests. */
#ifndef OLED_TEST_TASK_H
#define OLED_TEST_TASK_H
#include "freertos/FreeRTOS.h"
/** @brief Record the display startup delay without sleeping. */
void vTaskDelay(TickType_t ticks);
#endif
