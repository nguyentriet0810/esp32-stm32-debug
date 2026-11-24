/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file diskio_impl.h
 * @brief FAT disk-registration test interface.
 *
 * Exposes the minimal drive-allocation and unregister ABI used by probe_sd.c. Test-only header;
 * not used in firmware builds.
 */
#ifndef SD_TEST_DISKIO_IMPL_H
#define SD_TEST_DISKIO_IMPL_H
#include "esp_err.h"
#include "ff.h"
#define FF_DRV_NOT_USED 0xFF
typedef struct
{
    int token;
} ff_diskio_impl_t;
/**
 * @brief Allocate simulated FAT drive 1 or consume an injected allocation error.
 */
esp_err_t ff_diskio_get_drive(BYTE *drive);
/**
 * @brief Assert FAT/VFS teardown before unregistering the simulated disk.
 */
void ff_diskio_register(BYTE drive, const ff_diskio_impl_t *implementation);
#define ff_diskio_unregister(drive) ff_diskio_register(drive, NULL)
#endif
