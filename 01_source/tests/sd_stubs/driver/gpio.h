/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file gpio.h
 * @brief Card-detect GPIO test interface.
 *
 * Provides the GPIO configuration fields and calls checked by the active-low detection tests.
 * Test-only header; not used in firmware builds.
 */
#ifndef SD_TEST_GPIO_H
#define SD_TEST_GPIO_H
#include <stdint.h>
#include "esp_err.h"
typedef int gpio_num_t;
typedef struct
{
    uint64_t pin_bit_mask;
    int mode;
    int pull_up_en;
    int pull_down_en;
    int intr_type;
} gpio_config_t;
#define GPIO_MODE_INPUT 1
#define GPIO_PULLUP_ENABLE 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
/**
 * @brief Validate the SD detect GPIO and restore its simulated internal pull-up.
 */
esp_err_t gpio_config(const gpio_config_t *config);
/**
 * @brief Model active-low insertion and the missing-pull-up cleanup regression.
 */
int gpio_get_level(gpio_num_t pin);
#endif
