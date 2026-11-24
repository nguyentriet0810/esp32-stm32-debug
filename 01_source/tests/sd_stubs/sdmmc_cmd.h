/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file sdmmc_cmd.h
 * @brief Card geometry and host configuration test interface.
 *
 * Only fields read or populated by probe_sd.c are represented; no command protocol is emulated.
 * Test-only header; not used in firmware builds.
 */
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
/**
 * @brief Validate host timing and populate static geometry unless a failure is injected.
 */
esp_err_t sdmmc_card_init(const sdmmc_host_t *host, sdmmc_card_t *card);
#endif
