/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file semphr.h
 * @brief Mutex ownership test interface.
 *
 * The harness asserts locking order and injects acquisition failure without creating real tasks.
 * Test-only header; not used in firmware builds.
 */
#ifndef SD_TEST_SEMPHR_H
#define SD_TEST_SEMPHR_H
#include "freertos/FreeRTOS.h"
typedef void *SemaphoreHandle_t;
/**
 * @brief Return the address of the single harness-owned mutex token.
 */
SemaphoreHandle_t xSemaphoreCreateMutex(void);
/**
 * @brief Assert the bounded lock protocol or inject an unavailable-mutex result.
 */
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t ticks);
/**
 * @brief Assert ownership and release the simulated mutex.
 */
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex);
/**
 * @brief Assert that startup deletes only an unlocked harness-owned mutex.
 */
void vSemaphoreDelete(SemaphoreHandle_t mutex);
#endif
