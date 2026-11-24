/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_sd.h
 * @brief Serialized microSD SPI and FAT access API.
 *
 * The component owns SPI2 and mounts existing FAT media at /sdcard. Mutex-protected operations
 * bound file sizes and directory work; no API formats or deletes files.
 */
#ifndef PROBE_SD_H
#define PROBE_SD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/** @brief Bytes for a full 8.3 basename and its NUL terminator. */
#define PROBE_SD_NAME_CAPACITY 13U
/** @brief Maximum payload bytes accepted by a single read or write API call. */
#define PROBE_SD_MAX_IO_BYTES 256U
/** @brief Maximum root entries published by one list call. */
#define PROBE_SD_MAX_LIST_ENTRIES 8U

/** @brief Snapshot of card detection, mount ownership and the last operation result. */
typedef struct
{
    bool enabled;            /**< SD feature availability in this build. */
    bool present;            /**< Active-low card-detect state sampled under the mutex. */
    bool mounted;            /**< True while the component owns a mounted card descriptor. */
    uint64_t capacity_bytes; /**< Card capacity in bytes; zero without known geometry. */
    esp_err_t last_error;    /**< Last recorded operation or cleanup result. */
} probe_sd_status_t;

/** @brief Root entry with its complete supported 8.3 name and file size in bytes. */
typedef struct
{
    char name[PROBE_SD_NAME_CAPACITY]; /**< Complete, NUL-terminated 8.3 basename. */
    uint64_t size_bytes; /**< File size in bytes from validated filesystem metadata. */
    bool directory;      /**< True for a directory entry; listing never descends into it. */
} probe_sd_entry_t;

/**
 * @brief Initialize detection and serialization, then attempt a FAT mount at /sdcard.
 * @pre Call once from application startup before using other APIs, in task context.
 * @return ESP_OK when mounted; ESP_ERR_NOT_FOUND when no card is inserted, or an SDK error.
 * @note A failed mount never formats the card. Other APIs remain usable after a mount failure.
 *       All APIs return ESP_ERR_NOT_SUPPORTED when CONFIG_PROBE_SD_ENABLE is disabled.
 */
esp_err_t probe_sd_start(void);

/**
 * @brief Retry mounting an inserted card; an existing present mount is kept.
 * @return ESP_OK when mounted, ESP_ERR_NOT_FOUND for no card, or an SDK error.
 * @pre probe_sd_start has completed; task context only.
 * @note Serialized with all card operations; waits at most one second for the mutex.
 */
esp_err_t probe_sd_mount(void);

/**
 * @brief Unmount the card and release the component-owned SPI bus.
 * @return ESP_OK when all resources are released; an SDK error on cleanup failure.
 * @pre probe_sd_start has completed; task context only. Safe when already unmounted.
 */
esp_err_t probe_sd_unmount(void);

/**
 * @brief Read detection and mount state, releasing a removed card's stale mount first.
 * @param[out] status Caller-owned snapshot; written only on ESP_OK.
 * @return ESP_OK on a snapshot, ESP_ERR_INVALID_ARG for NULL, or a lock/state error.
 * @note Does not mount a newly inserted card; task context only.
 */
esp_err_t probe_sd_get_status(probe_sd_status_t *status);

/**
 * @brief List a bounded set of supported root entries without modifying the filesystem.
 * @param[out] entries Caller-owned array of at least capacity entries.
 * @param[in] capacity Number of entries, 1..PROBE_SD_MAX_LIST_ENTRIES.
 * @param[out] count Number of complete entries written, initialized to zero on failure.
 * @param[out] truncated True if more entries exist or a name/file size was unsupported.
 * @return ESP_OK on completion, ESP_ERR_INVALID_ARG for invalid buffers, or a card/I/O error.
 * @note Scans at most 32 directory records; no directories are traversed. Task context only.
 */
esp_err_t probe_sd_list(probe_sd_entry_t *entries, size_t capacity, size_t *count, bool *truncated);

/**
 * @brief Read the beginning of one root file with a safe 8.3 basename.
 * @param[in] name ASCII letters/digits/_/- in a 1..8 character stem and optional 1..3 extension.
 * @param[out] data Caller-owned buffer of capacity bytes; no terminator is appended.
 * @param[in] capacity Buffer size, 1..PROBE_SD_MAX_IO_BYTES.
 * @param[out] length Bytes read, initialized to zero on failure.
 * @param[out] truncated True if the file contains additional bytes.
 * @return ESP_OK on completion, ESP_ERR_NOT_FOUND for a missing card/file, or an I/O error.
 * @retval ESP_ERR_NOT_SUPPORTED The file size cannot be represented by the target's off_t.
 * @note No paths or subdirectories are accepted; task context only.
 */
esp_err_t probe_sd_read(const char *name, void *data, size_t capacity, size_t *length,
                        bool *truncated);

/**
 * @brief Create one new root file, then synchronize and close it; existing files are preserved.
 * @param[in] name Safe 8.3 basename with the same rules as probe_sd_read.
 * @param[in] data Caller-owned payload; may be NULL only when length is zero.
 * @param[in] length Payload length, 0..PROBE_SD_MAX_IO_BYTES.
 * @return ESP_OK after write/fsync/close, ESP_ERR_INVALID_STATE if the name exists, or an I/O
 * error.
 * @note Failed writes can leave a partial new file for inspection; no file is deleted.
 *       No API formats, overwrites or removes files. Task context only.
 */
esp_err_t probe_sd_write(const char *name, const void *data, size_t length);

#endif /* PROBE_SD_H */
