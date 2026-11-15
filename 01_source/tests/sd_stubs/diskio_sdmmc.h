/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SD_TEST_DISKIO_SDMMC_H
#define SD_TEST_DISKIO_SDMMC_H
#include <stdbool.h>
#include "ff.h"
#include "sdmmc_cmd.h"
void ff_diskio_register_sdmmc(BYTE drive, sdmmc_card_t *card);
void ff_sdmmc_set_disk_status_check(BYTE drive, bool enabled);
#endif
