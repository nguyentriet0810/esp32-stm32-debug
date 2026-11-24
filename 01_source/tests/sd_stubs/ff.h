/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file ff.h
 * @brief FatFs mount and error subset for host tests.
 *
 * Supports injected FAT mount and cleanup outcomes without implementing a filesystem. Test-only
 * header; not used in firmware builds.
 */
#ifndef SD_TEST_FATFS_H
#define SD_TEST_FATFS_H
#define FF_VOLUMES 2
typedef unsigned char BYTE;
typedef char TCHAR;
typedef struct
{
    int token;
} FATFS;
typedef enum
{
    FR_OK = 0,
    FR_DISK_ERR = 1,
    FR_INT_ERR = 2,
    FR_NOT_READY = 3,
    FR_NO_FILESYSTEM = 13,
    FR_NOT_ENOUGH_CORE = 17,
    FR_TIMEOUT = 15
} FRESULT;
/**
 * @brief Model FAT mount ownership and one-shot mount/unmount failures.
 */
FRESULT f_mount(FATFS *filesystem, const TCHAR *drive, BYTE immediate);
#endif
