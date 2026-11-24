/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file sd_test.c
 * @brief Host microSD lifecycle and fault-injection test harness.
 *
 * Includes the production SD implementation behind in-memory SDK and filesystem doubles.
 * Ownership assertions and injected failures check cleanup without accessing physical media.
 */
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#include "esp_vfs_fat.h"
#include "freertos/semphr.h"

/**
 * @brief Emulate one open descriptor and enforce create-new writes without overwrite flags.
 */
static int test_open(const char *path, int flags, ...);
/**
 * @brief Copy fixture bytes and optionally simulate card removal during an open read.
 */
static ssize_t test_read(int fd, void *data, size_t length);
/**
 * @brief Write fixture bytes with an optional short-write result.
 */
static ssize_t test_write(int fd, const void *data, size_t length);
/**
 * @brief Release the single simulated file descriptor and count balanced closes.
 */
static int test_close(int fd);
/**
 * @brief Count synchronization attempts and return the configured injected outcome.
 */
static int test_fsync(int fd);
/**
 * @brief Return metadata for the currently open fixture descriptor.
 */
static int test_fstat(int fd, struct stat *metadata);
/**
 * @brief Return fixture metadata by path, or model ENOENT.
 */
static int test_stat(const char *path, struct stat *metadata);
/**
 * @brief Reset root enumeration after asserting card and bus ownership.
 */
static DIR *test_opendir(const char *path);
/**
 * @brief Return fixture names in insertion order using one reused directory entry.
 */
static struct dirent *test_readdir(DIR *directory);
/**
 * @brief Validate the simulated root-directory handle without filesystem I/O.
 */
static int test_closedir(DIR *directory);

/* Keep the real filesystem untouched. These shims affect production calls only. */
#define open(...) test_open(__VA_ARGS__)
#define read(...) test_read(__VA_ARGS__)
#define write(...) test_write(__VA_ARGS__)
#define close(...) test_close(__VA_ARGS__)
#define fsync(...) test_fsync(__VA_ARGS__)
#define fstat(...) test_fstat(__VA_ARGS__)
#define stat(...) test_stat(__VA_ARGS__)
#define opendir(...) test_opendir(__VA_ARGS__)
#define readdir(...) test_readdir(__VA_ARGS__)
#define closedir(...) test_closedir(__VA_ARGS__)
#include "../components/probe_sd/probe_sd.c"
#undef open
#undef read
#undef write
#undef close
#undef fsync
#undef fstat
#undef stat
#undef opendir
#undef readdir
#undef closedir

/** @brief In-memory file fixture, including deliberately oversized/unsupported entries. */
typedef struct
{
    char name[64];
    uint8_t bytes[PROBE_SD_MAX_IO_BYTES + 4U];
    size_t length;
    bool directory;
} test_file_t;

/* Fixtures, resource flags and fault switches are owned by this single-threaded harness. */
static test_file_t files[16];
static size_t file_count;
static size_t directory_index;
static struct dirent directory_entry;
static int current_file = -1;
static size_t file_offset;
static unsigned int open_count;
static unsigned int close_count;
static unsigned int sync_count;
static unsigned int bus_initializations;
static unsigned int bus_releases;
static unsigned int mount_attempts;
static unsigned int unmounts;
static unsigned int detect_configurations;
static bool inserted;
static bool detect_pullup;
static bool bus_owned;
static bool mutex_locked;
static bool mutex_available = true;
static int mutex_token;
static esp_err_t next_mount_result = ESP_OK;
static bool fail_sync;
static bool short_write;
static bool fail_bus_release;
static bool remove_during_read;
static sdmmc_card_t test_card = {.csd = {.capacity = 1048576U, .sector_size = 512U}};
static bool device_owned;
static bool disk_registered;
static bool vfs_registered;
static bool fat_registered;
static bool foreign_vfs;
static sdmmc_card_t *active_card;
static FATFS test_filesystem;
static FATFS foreign_filesystem;
static esp_err_t next_drive_result = ESP_OK;
static esp_err_t next_vfs_result = ESP_OK;
static FRESULT next_fat_result = FR_OK;
static bool fail_device_release;
static bool fail_vfs_release;
static bool fail_fat_release;

/**
 * @brief Return a fixed diagnostic label; numeric error identity is asserted separately.
 */
const char *esp_err_to_name(esp_err_t error)
{
    (void)error;
    return "mock error";
}

/**
 * @brief Return the address of the single harness-owned mutex token.
 */
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    return &mutex_token;
}

/**
 * @brief Assert the bounded lock protocol or inject an unavailable-mutex result.
 */
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t ticks)
{
    assert(mutex == &mutex_token);
    assert(ticks <= 1000U);
    if (!mutex_available)
    {
        return pdFALSE;
    }
    assert(!mutex_locked);
    mutex_locked = true;
    return pdTRUE;
}

/**
 * @brief Assert ownership and release the simulated mutex.
 */
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    assert(mutex == &mutex_token);
    assert(mutex_locked);
    mutex_locked = false;
    return pdTRUE;
}

/**
 * @brief Assert that startup deletes only an unlocked harness-owned mutex.
 */
void vSemaphoreDelete(SemaphoreHandle_t mutex)
{
    assert(mutex == &mutex_token);
    assert(!mutex_locked);
}

/**
 * @brief Validate the SD detect GPIO and restore its simulated internal pull-up.
 */
esp_err_t gpio_config(const gpio_config_t *config)
{
    assert(config->pin_bit_mask == (UINT64_C(1) << 42U));
    assert(config->mode == GPIO_MODE_INPUT);
    assert(config->pull_up_en == GPIO_PULLUP_ENABLE);
    detect_pullup = true;
    ++detect_configurations;
    return ESP_OK;
}

/**
 * @brief Model active-low insertion and the missing-pull-up cleanup regression.
 */
int gpio_get_level(gpio_num_t pin)
{
    assert(pin == 42);
    /* A missing DNP detect pullup reads as insertion unless software restores it. */
    return (inserted || !detect_pullup) ? 0 : 1;
}

/**
 * @brief Assert PCB SPI2 pins and exclusive bus acquisition before recording ownership.
 */
esp_err_t spi_bus_initialize(spi_host_device_t host, const spi_bus_config_t *config, int dma)
{
    assert(host == SPI2_HOST);
    assert(dma == SPI_DMA_CH_AUTO);
    assert(config->mosi_io_num == 39 && config->miso_io_num == 41 && config->sclk_io_num == 40);
    assert(config->max_transfer_sz >= 512);
    assert(!bus_owned);
    bus_owned = true;
    ++bus_initializations;
    return ESP_OK;
}

/**
 * @brief Require device teardown before releasing SPI2, with an optional one-shot failure.
 */
esp_err_t spi_bus_free(spi_host_device_t host)
{
    assert(host == SPI2_HOST);
    assert(bus_owned);
    assert(!device_owned);
    if (fail_bus_release)
    {
        fail_bus_release = false;
        return ESP_FAIL;
    }
    bus_owned = false;
    ++bus_releases;
    return ESP_OK;
}

/**
 * @brief Provide a successful no-allocation SDSPI host initialization test double.
 */
esp_err_t sdspi_host_init(void)
{
    return ESP_OK;
}

/**
 * @brief Validate device pins and transfer one simulated SDSPI handle to the caller.
 */
esp_err_t sdspi_host_init_device(const sdspi_device_config_t *device, sdspi_dev_handle_t *handle)
{
    assert(device->gpio_cs == 38 && device->gpio_cd == 42 && device->gpio_wp == -1);
    assert(device->host_id == SPI2_HOST);
    assert(!device_owned);
    assert(bus_owned && inserted);
    device_owned = true;
    *handle = 17;
    return ESP_OK;
}

/**
 * @brief Validate host timing and populate static geometry unless a failure is injected.
 */
esp_err_t sdmmc_card_init(const sdmmc_host_t *host, sdmmc_card_t *card)
{
    assert(device_owned && host->slot == 17 && host->max_freq_khz == 4000);
    assert(host->command_timeout_ms > 0 && host->command_timeout_ms <= 1000);
    ++mount_attempts;
    const esp_err_t result = next_mount_result;
    next_mount_result = ESP_OK;
    if (result == ESP_OK)
    {
        *card = test_card;
        card->host = *host;
        active_card = card;
    }
    return result;
}

/**
 * @brief Verify reverse cleanup order and model the SDK clearing the detect pull-up.
 */
esp_err_t sdspi_host_remove_device(sdspi_dev_handle_t handle)
{
    assert(handle == 17 && device_owned);
    assert(!disk_registered && !vfs_registered && !fat_registered);
    assert(current_file == -1);
    if (fail_device_release)
    {
        fail_device_release = false;
        return ESP_FAIL;
    }
    device_owned = false;
    active_card = NULL;
    /* IDF's device cleanup disables the detect pullup; production must repair it. */
    detect_pullup = false;
    return ESP_OK;
}

/**
 * @brief Allocate simulated FAT drive 1 or consume an injected allocation error.
 */
esp_err_t ff_diskio_get_drive(BYTE *drive)
{
    assert(device_owned && !disk_registered);
    const esp_err_t result = next_drive_result;
    next_drive_result = ESP_OK;
    if (result == ESP_OK)
    {
        *drive = 1U;
    }
    return result;
}

/**
 * @brief Assert card identity and record disk-I/O registration ownership.
 */
void ff_diskio_register_sdmmc(BYTE drive, sdmmc_card_t *card)
{
    assert(drive == 1U && card == active_card && !disk_registered);
    disk_registered = true;
}

/**
 * @brief Assert that card-removal checking is enabled for the registered disk.
 */
void ff_sdmmc_set_disk_status_check(BYTE drive, bool enabled)
{
    assert(drive == 1U && disk_registered && enabled);
}

/**
 * @brief Assert FAT/VFS teardown before unregistering the simulated disk.
 */
void ff_diskio_register(BYTE drive, const ff_diskio_impl_t *implementation)
{
    assert(drive == 1U && implementation == NULL && disk_registered);
    assert(!fat_registered && !vfs_registered);
    disk_registered = false;
}

/**
 * @brief Model VFS registration, including a foreign filesystem on INVALID_STATE.
 */
esp_err_t esp_vfs_fat_register(const esp_vfs_fat_conf_t *configuration, FATFS **filesystem)
{
    assert(strcmp(configuration->base_path, "/sdcard") == 0);
    assert(strcmp(configuration->fat_drive, "1:") == 0 && configuration->max_files > 0U);
    assert(disk_registered && !vfs_registered);
    const esp_err_t result = next_vfs_result;
    next_vfs_result = ESP_OK;
    if (result == ESP_OK)
    {
        vfs_registered = true;
        *filesystem = &test_filesystem;
    }
    else if (result == ESP_ERR_INVALID_STATE)
    {
        /* SDK returns an existing FS pointer; production must never take ownership. */
        foreign_vfs = true;
        *filesystem = &foreign_filesystem;
    }
    return result;
}

/**
 * @brief Release simulated VFS ownership and optionally report a cleanup error.
 */
esp_err_t esp_vfs_fat_unregister_path(const char *path)
{
    assert(strcmp(path, "/sdcard") == 0 && vfs_registered);
    assert(!fat_registered);
    vfs_registered = false;
    if (fail_vfs_release)
    {
        fail_vfs_release = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Model FAT mount ownership and one-shot mount/unmount failures.
 */
FRESULT f_mount(FATFS *filesystem, const TCHAR *drive, BYTE immediate)
{
    assert(strcmp(drive, "1:") == 0);
    if (filesystem == NULL)
    {
        assert(fat_registered && immediate == 0U && current_file == -1);
        if (fail_fat_release)
        {
            fail_fat_release = false;
            return FR_DISK_ERR;
        }
        fat_registered = false;
        ++unmounts;
        return FR_OK;
    }
    assert(filesystem == &test_filesystem && vfs_registered && disk_registered);
    assert(immediate == 1U && !fat_registered);
    fat_registered = true;
    const FRESULT result = next_fat_result;
    next_fat_result = FR_OK;
    return result;
}

/**
 * @brief Compare two fixture basenames with FAT-like case-insensitive ASCII matching.
 */
static bool same_name(const char *left, const char *right)
{
    size_t index = 0U;
    while ((left[index] != '\0') && (right[index] != '\0'))
    {
        if (toupper((unsigned char)left[index]) != toupper((unsigned char)right[index]))
        {
            return false;
        }
        ++index;
    }
    return left[index] == right[index];
}

/**
 * @brief Find a fixture by /sdcard basename without touching the host filesystem.
 */
static int find_file(const char *path)
{
    assert(strncmp(path, "/sdcard/", 8U) == 0);
    for (size_t index = 0U; index < file_count; ++index)
    {
        if (same_name(files[index].name, &path[8]))
        {
            return (int)index;
        }
    }
    return -1;
}

/**
 * @brief Copy a bounded file or directory fixture into harness-owned storage.
 */
static void add_file(const char *name, const void *data, size_t length, bool directory)
{
    assert(file_count < (sizeof(files) / sizeof(files[0])));
    test_file_t *file = &files[file_count++];
    assert(strlen(name) < sizeof(file->name));
    (void)strcpy(file->name, name);
    assert(length <= sizeof(file->bytes));
    if (length != 0U)
    {
        (void)memcpy(file->bytes, data, length);
    }
    file->length = length;
    file->directory = directory;
}

/** @brief Emulate one descriptor and enforce create-new writes without overwrite flags. */
static int test_open(const char *path, int flags, ...)
{
    assert(current_file == -1 && inserted && bus_owned);
    ++open_count;
    int index = find_file(path);
    if ((flags & O_WRONLY) != 0)
    {
        assert((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL));
        assert((flags & (O_TRUNC | O_APPEND)) == 0);
        if (index >= 0)
        {
            errno = EEXIST;
            return -1;
        }
        index = (int)file_count;
        add_file(&path[8], NULL, 0U, false);
    }
    else if (index < 0)
    {
        errno = ENOENT;
        return -1;
    }
    current_file = index;
    file_offset = 0U;
    return 3;
}

/** @brief Read fixture bytes and optionally inject card removal while the handle is open. */
static ssize_t test_read(int fd, void *data, size_t length)
{
    assert(fd == 3 && current_file >= 0);
    const test_file_t *file = &files[(size_t)current_file];
    const size_t remaining = file->length - file_offset;
    const size_t count = (length < remaining) ? length : remaining;
    (void)memcpy(data, &file->bytes[file_offset], count);
    file_offset += count;
    if (remove_during_read)
    {
        inserted = false;
        remove_during_read = false;
    }
    return (ssize_t)count;
}

/** @brief Write fixture bytes with an optional short-write result. */
static ssize_t test_write(int fd, const void *data, size_t length)
{
    assert(fd == 3 && current_file >= 0);
    test_file_t *file = &files[(size_t)current_file];
    size_t count = length;
    if (short_write && (count > 0U))
    {
        --count;
    }
    assert(file_offset + count <= sizeof(file->bytes));
    (void)memcpy(&file->bytes[file_offset], data, count);
    file_offset += count;
    file->length = file_offset;
    return (ssize_t)count;
}

/** @brief Release the simulated descriptor and record its balanced close. */
static int test_close(int fd)
{
    assert(fd == 3 && current_file >= 0);
    current_file = -1;
    ++close_count;
    return 0;
}

/** @brief Count synchronization attempts and return the configured fault outcome. */
static int test_fsync(int fd)
{
    assert(fd == 3 && current_file >= 0);
    ++sync_count;
    return fail_sync ? -1 : 0;
}

/**
 * @brief Populate only fixture size and file/directory mode in a stat result.
 */
static void metadata_for(const test_file_t *file, struct stat *metadata)
{
    (void)memset(metadata, 0, sizeof(*metadata));
    metadata->st_size = (off_t)file->length;
    metadata->st_mode = file->directory ? S_IFDIR : S_IFREG;
}

/** @brief Populate metadata for the currently open fixture. */
static int test_fstat(int fd, struct stat *metadata)
{
    assert(fd == 3 && current_file >= 0);
    metadata_for(&files[(size_t)current_file], metadata);
    return 0;
}

/** @brief Look up fixture metadata by path, modeling ENOENT when absent. */
static int test_stat(const char *path, struct stat *metadata)
{
    const int index = find_file(path);
    if (index < 0)
    {
        errno = ENOENT;
        return -1;
    }
    metadata_for(&files[(size_t)index], metadata);
    return 0;
}

/** @brief Begin root enumeration after validating card and bus ownership. */
static DIR *test_opendir(const char *path)
{
    assert(strcmp(path, "/sdcard") == 0);
    assert(inserted && bus_owned);
    directory_index = 0U;
    return (DIR *)&directory_entry;
}

/** @brief Reuse one dirent to return fixture names in insertion order. */
static struct dirent *test_readdir(DIR *directory)
{
    assert(directory == (DIR *)&directory_entry);
    if (directory_index >= file_count)
    {
        return NULL;
    }
    (void)strcpy(directory_entry.d_name, files[directory_index++].name);
    return &directory_entry;
}

/** @brief Validate closing the simulated root-directory handle. */
static int test_closedir(DIR *directory)
{
    assert(directory == (DIR *)&directory_entry);
    return 0;
}

#if CONFIG_PROBE_SD_ENABLE
/**
 * @brief Exercise absent-card startup, insertion, repeated mount and unmount ownership.
 */
static void test_lifecycle(void)
{
    probe_sd_status_t status = {0};
    assert(probe_sd_start() == ESP_ERR_NOT_FOUND);
    assert(bus_initializations == 0U && mount_attempts == 0U);
    assert(probe_sd_get_status(&status) == ESP_OK);
    assert(status.enabled && !status.present && !status.mounted && status.capacity_bytes == 0U);

    inserted = true;
    next_mount_result = ESP_FAIL;
    assert(probe_sd_mount() == ESP_FAIL);
    assert(!bus_owned && bus_initializations == 1U && bus_releases == 1U);
    assert(detect_pullup);
    assert(probe_sd_get_status(&status) == ESP_OK && !status.mounted);
    assert(probe_sd_mount() == ESP_OK);
    assert(probe_sd_get_status(&status) == ESP_OK);
    assert(status.present && status.mounted && status.capacity_bytes == UINT64_C(536870912));
    assert(probe_sd_mount() == ESP_OK && mount_attempts == 2U);
    assert(probe_sd_start() == ESP_ERR_INVALID_STATE);
    (void)printf("PASS: no-card startup, failed mount cleanup and insertion retry\n");
}

/**
 * @brief Exercise filename bounds, listing, bounded reads and create-new write failures.
 */
static void test_files(void)
{
    uint8_t bytes[PROBE_SD_MAX_IO_BYTES + 1U] = {0};
    size_t length = 99U;
    bool truncated = true;
    const uint8_t binary[] = {0U, 1U, 127U, 255U};
    add_file("DATA.BIN", binary, sizeof(binary), false);
    add_file("SUBDIR", NULL, 0U, true);
    add_file("LONG-FILENAME.TXT", NULL, 0U, false);

    const unsigned int before = open_count;
    const char *invalid[] = {"../BAD",        "/BAD",   "A/B",  "A\\B",    ".", "..",
                             "TOOLONG99.TXT", "X.YYYY", "A..B", "A B.TXT", ""};
    for (size_t index = 0U; index < (sizeof(invalid) / sizeof(invalid[0])); ++index)
    {
        assert(probe_sd_write(invalid[index], binary, sizeof(binary)) == ESP_ERR_INVALID_ARG);
        assert(probe_sd_read(invalid[index], bytes, sizeof(binary), &length, &truncated) ==
               ESP_ERR_INVALID_ARG);
    }
    assert(open_count == before);
    assert(probe_sd_read("DATA.BIN", bytes, sizeof(bytes), &length, &truncated) ==
           ESP_ERR_INVALID_ARG);
    assert(probe_sd_write("BIG.BIN", bytes, sizeof(bytes)) == ESP_ERR_INVALID_ARG);
    assert(probe_sd_read("DATA.BIN", bytes, 2U, &length, &truncated) == ESP_OK);
    assert(length == 2U && truncated && memcmp(bytes, binary, 2U) == 0);
    assert(probe_sd_read("DATA.BIN", bytes, sizeof(bytes) - 1U, &length, &truncated) == ESP_OK);
    assert(length == sizeof(binary) && !truncated && memcmp(bytes, binary, sizeof(binary)) == 0);
    assert(probe_sd_read("MISSING.TXT", bytes, sizeof(binary), &length, &truncated) ==
           ESP_ERR_NOT_FOUND);

    assert(probe_sd_write("NEW.TXT", binary, sizeof(binary)) == ESP_OK);
    assert(sync_count == 1U && current_file == -1);
    assert(probe_sd_write("new.txt", "changed", 7U) == ESP_ERR_INVALID_STATE);
    assert(probe_sd_read("NEW.TXT", bytes, sizeof(binary), &length, &truncated) == ESP_OK);
    assert(memcmp(bytes, binary, sizeof(binary)) == 0);
    assert(probe_sd_write("EMPTY", NULL, 0U) == ESP_OK);

    fail_sync = true;
    assert(probe_sd_write("NOSYNC.TXT", binary, sizeof(binary)) != ESP_OK);
    assert(current_file == -1);
    fail_sync = false;
    short_write = true;
    assert(probe_sd_write("SHORT.TXT", binary, sizeof(binary)) != ESP_OK);
    assert(current_file == -1);
    short_write = false;
    assert(close_count >= 7U);

    probe_sd_entry_t entries[PROBE_SD_MAX_LIST_ENTRIES];
    size_t count = 0U;
    assert(probe_sd_list(entries, PROBE_SD_MAX_LIST_ENTRIES, &count, &truncated) == ESP_OK);
    assert(truncated && count == file_count - 1U);
    for (size_t index = 0U; index < count; ++index)
    {
        assert(strlen(entries[index].name) < PROBE_SD_NAME_CAPACITY);
    }
    (void)printf("PASS: binary read bounds, path rejection, exclusive create and I/O failures\n");
}

/**
 * @brief Exercise removed-card detection and bounded mutex acquisition failure.
 */
static void test_removal_and_lock(void)
{
    const unsigned int previous_unmounts = unmounts;
    probe_sd_status_t status = {.capacity_bytes = UINT64_C(1234)};
    mutex_available = false;
    assert(probe_sd_get_status(&status) == ESP_ERR_TIMEOUT);
    assert(status.capacity_bytes == UINT64_C(1234));
    mutex_available = true;
    inserted = false;
    assert(probe_sd_get_status(&status) == ESP_OK);
    assert(!status.present && !status.mounted && status.capacity_bytes == 0U);
    assert(!bus_owned && unmounts == previous_unmounts + 1U && detect_pullup);
    uint8_t byte = 0U;
    size_t length = 0U;
    bool truncated = false;
    const unsigned int before = open_count;
    assert(probe_sd_read("DATA.BIN", &byte, 1U, &length, &truncated) == ESP_ERR_NOT_FOUND);
    assert(open_count == before);
    inserted = true;
    assert(probe_sd_mount() == ESP_OK);
    assert(probe_sd_unmount() == ESP_OK && !bus_owned && detect_pullup);
    assert(probe_sd_unmount() == ESP_OK);
    assert(unmounts == previous_unmounts + 2U && bus_initializations == bus_releases);
    assert(detect_configurations >= 4U && !mutex_locked);
    (void)printf(
        "PASS: bounded lock wait, removal invalidation, reinsert and idempotent unmount\n");
}

/**
 * @brief Check handle closure after removal and retryable cleanup ownership.
 */
static void test_mid_io_removal_and_cleanup_retry(void)
{
    uint8_t bytes[4] = {0};
    size_t length = 99U;
    bool truncated = true;
    assert(probe_sd_mount() == ESP_OK);
    remove_during_read = true;
    assert(probe_sd_read("DATA.BIN", bytes, sizeof(bytes), &length, &truncated) ==
           ESP_ERR_NOT_FOUND);
    assert(length == 0U && !truncated && current_file == -1 && !bus_owned && detect_pullup);

    inserted = true;
    assert(probe_sd_mount() == ESP_OK);
    fail_bus_release = true;
    assert(probe_sd_unmount() == ESP_FAIL);
    assert(bus_owned);
    probe_sd_status_t status = {0};
    assert(probe_sd_get_status(&status) == ESP_OK && !status.mounted);
    /* A failed SPI free must be retried before allocating the same host again. */
    assert(probe_sd_mount() == ESP_OK);
    assert(probe_sd_unmount() == ESP_OK);
    assert(!bus_owned && bus_initializations == bus_releases && !mutex_locked);
    (void)printf(
        "PASS: removal during I/O clears output; failed SPI cleanup preserves retry ownership\n");
}

/**
 * @brief Inject failures at mount stages and verify ownership is not leaked or stolen.
 */
static void test_mount_stage_failures(void)
{
    for (unsigned int stage = 0U; stage < 5U; ++stage)
    {
        if (stage == 0U)
        {
            next_drive_result = ESP_ERR_NOT_FOUND;
        }
        else if (stage == 1U)
        {
            next_vfs_result = ESP_ERR_NO_MEM;
        }
        else if (stage == 2U)
        {
            next_vfs_result = ESP_ERR_INVALID_STATE;
        }
        else
        {
            next_fat_result = (stage == 3U) ? FR_NO_FILESYSTEM : FR_NOT_ENOUGH_CORE;
        }
        assert(probe_sd_mount() != ESP_OK);
        assert(!bus_owned && !device_owned && !disk_registered && !vfs_registered &&
               !fat_registered);
        probe_sd_status_t status = {0};
        assert(probe_sd_get_status(&status) == ESP_OK);
        assert(status.present && !status.mounted && status.capacity_bytes == 0U);
        assert(detect_pullup);
        assert(probe_sd_mount() == ESP_OK);
        assert(probe_sd_unmount() == ESP_OK);
    }
    assert(foreign_vfs);

    assert(probe_sd_mount() == ESP_OK);
    fail_fat_release = true;
    assert(probe_sd_unmount() != ESP_OK);
    assert(bus_owned && device_owned && disk_registered && vfs_registered && fat_registered);
    assert(probe_sd_mount() == ESP_OK);
    assert(probe_sd_unmount() == ESP_OK);

    assert(probe_sd_mount() == ESP_OK);
    fail_device_release = true;
    assert(probe_sd_unmount() != ESP_OK);
    assert(device_owned && bus_owned && !disk_registered && !vfs_registered && !fat_registered);
    assert(probe_sd_mount() == ESP_OK);
    assert(probe_sd_unmount() == ESP_OK);

    assert(probe_sd_mount() == ESP_OK);
    fail_vfs_release = true;
    assert(probe_sd_unmount() != ESP_OK);
    assert(!bus_owned && !device_owned && !disk_registered && !vfs_registered && !fat_registered);
    assert(probe_sd_mount() == ESP_OK);
    assert(probe_sd_unmount() == ESP_OK);
    assert(bus_initializations == bus_releases && !mutex_locked);
    (void)printf(
        "PASS: disk-slot, VFS allocation/conflict, FAT mount and cleanup failures recover\n");
}
#endif

/**
 * @brief Run the enabled/disabled SD test scenario selected at compile time.
 * @return Zero when all assertions pass; assertion failure terminates the process.
 */
int main(void)
{
#if CONFIG_PROBE_SD_ENABLE
    test_lifecycle();
    test_files();
    test_removal_and_lock();
    test_mid_io_removal_and_cleanup_retry();
    test_mount_stage_failures();
#else
    probe_sd_status_t status = {0};
    probe_sd_entry_t entry = {0};
    uint8_t byte = 0U;
    size_t count = 0U;
    bool truncated = false;
    assert(probe_sd_start() == ESP_ERR_NOT_SUPPORTED);
    assert(probe_sd_mount() == ESP_ERR_NOT_SUPPORTED);
    assert(probe_sd_unmount() == ESP_ERR_NOT_SUPPORTED);
    assert(probe_sd_get_status(&status) == ESP_ERR_NOT_SUPPORTED);
    assert(probe_sd_list(&entry, 1U, &count, &truncated) == ESP_ERR_NOT_SUPPORTED);
    assert(probe_sd_read("X.TXT", &byte, 1U, &count, &truncated) == ESP_ERR_NOT_SUPPORTED);
    assert(probe_sd_write("X.TXT", &byte, 1U) == ESP_ERR_NOT_SUPPORTED);
    assert(bus_initializations == 0U && mount_attempts == 0U && open_count == 0U);
    (void)printf("PASS: disabled component makes no GPIO, SPI or filesystem calls\n");
#endif
    return 0;
}
