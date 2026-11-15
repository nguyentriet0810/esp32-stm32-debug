/* SPDX-License-Identifier: GPL-3.0-or-later */
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
esp_err_t esp_vfs_fat_register(const esp_vfs_fat_conf_t *configuration, FATFS **filesystem);
esp_err_t esp_vfs_fat_unregister_path(const char *path);
#endif
