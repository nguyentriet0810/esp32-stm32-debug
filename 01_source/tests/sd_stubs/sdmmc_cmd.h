/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SD_TEST_SDMMC_H
#define SD_TEST_SDMMC_H
#include <stdint.h>
#include "esp_err.h"
typedef struct
{
    int slot;
    int max_freq_khz;
    int command_timeout_ms;
} sdmmc_host_t;
typedef struct
{
    struct
    {
        uint32_t capacity;
        uint32_t sector_size;
    } csd;
    sdmmc_host_t host;
} sdmmc_card_t;
esp_err_t sdmmc_card_init(const sdmmc_host_t *host, sdmmc_card_t *card);
#endif
