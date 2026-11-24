/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file esp_vfs_fat.h
 * @brief FAT VFS registration test interface.
 *
 * Models mount ownership and configuration while all file data stays in the test harness. Test-
 * only header; not used in firmware builds.
 */
#ifndef SD_TEST_VFS_H
#define SD_TEST_VFS_H
#include <stdbool.h>
#include <stddef.h>
#include "driver/sdspi_host.h"
#include "ff.h"
typedef struct
{
    const char *base_path;
    const char *fat_drive;
    size_t max_files;
} esp_vfs_fat_conf_t;
/**
 * @brief Model VFS registration, including a foreign filesystem on INVALID_STATE.
 */
esp_err_t esp_vfs_fat_register(const esp_vfs_fat_conf_t *configuration, FATFS **filesystem);
/**
 * @brief Release simulated VFS ownership and optionally report a cleanup error.
 */
esp_err_t esp_vfs_fat_unregister_path(const char *path);
#endif
