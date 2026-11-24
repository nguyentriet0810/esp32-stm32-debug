/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file diskio_sdmmc.h
 * @brief SDMMC disk-registration test interface.
 *
 * Declares registration hooks implemented by the SD lifecycle test harness. Test-only header;
 * not used in firmware builds.
 */
#ifndef SD_TEST_DISKIO_SDMMC_H
#define SD_TEST_DISKIO_SDMMC_H
#include <stdbool.h>
#include "ff.h"
#include "sdmmc_cmd.h"
/**
 * @brief Assert card identity and record disk-I/O registration ownership.
 */
void ff_diskio_register_sdmmc(BYTE drive, sdmmc_card_t *card);
/**
 * @brief Assert that card-removal checking is enabled for the registered disk.
 */
void ff_sdmmc_set_disk_status_check(BYTE drive, bool enabled);
#endif
