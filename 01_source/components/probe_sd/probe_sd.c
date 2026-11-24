/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_sd.c
 * @brief Serialized microSD SPI and FAT access.
 *
 * The component owns SPI2 and mounts existing FAT media at /sdcard. Mutex-protected operations
 * bound file sizes and directory work; no API formats or deletes files.
 */
#include "probe_sd.h"

#include "sdkconfig.h"
#if CONFIG_PROBE_SD_ENABLE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdmmc_cmd.h"

static const char sd_mount_path[] = "/sdcard";
/* After startup, the mutex serializes every resource flag, handle and card operation. */
static SemaphoreHandle_t sd_mutex;
static sdmmc_card_t sd_card_storage;
static sdmmc_card_t *sd_card;
static bool sd_bus_initialized;
static bool sd_device_initialized;
static sdspi_dev_handle_t sd_device;
static BYTE sd_drive = FF_DRV_NOT_USED;
static char sd_fat_drive[3];
static bool sd_disk_registered;
static bool sd_vfs_registered;
static bool sd_fat_registered;
static FATFS *sd_filesystem;
static esp_err_t sd_last_error = ESP_ERR_INVALID_STATE;

/**
 * @brief Enable the internal card-detect pull-up when the PCB external pull-up is DNP.
 */
static esp_err_t sd_configure_detect(void)
{
    const gpio_config_t detect = {.pin_bit_mask = UINT64_C(1) << CONFIG_PROBE_SD_DETECT_GPIO,
                                  .mode = GPIO_MODE_INPUT,
                                  .pull_up_en = GPIO_PULLUP_ENABLE,
                                  .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                  .intr_type = GPIO_INTR_DISABLE};
    return gpio_config(&detect);
}

/**
 * @brief Read the active-low mechanical card-detect contact.
 * @return true when the configured detect GPIO reads low.
 */
static bool sd_present(void)
{
    return gpio_get_level((gpio_num_t)CONFIG_PROBE_SD_DETECT_GPIO) == 0;
}

/**
 * @brief Acquire the SD mutex with a one-second maximum task wait.
 * @return ESP_OK, ESP_ERR_TIMEOUT or ESP_ERR_INVALID_STATE before initialization.
 */
static esp_err_t sd_lock(void)
{
    esp_err_t result = ESP_ERR_INVALID_STATE;
    if (sd_mutex != NULL)
    {
        result =
            (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(1000U)) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
    }
    return result;
}

/**
 * @brief Check whether any completed mount stage still needs cleanup.
 * @return true if bus, device, disk, VFS or FAT ownership remains.
 * @pre Caller holds the SD mutex.
 */
static bool sd_resources_owned(void)
{
    return sd_bus_initialized || sd_device_initialized || sd_disk_registered || sd_vfs_registered ||
           sd_fat_registered;
}

/**
 * @brief Release completed ownership stages in reverse dependency order.
 * @return ESP_OK when cleanup succeeds; otherwise the recorded cleanup error.
 * @pre Caller holds the SD mutex, or startup has exclusive access.
 * @note Stages still owned after a failure remain available for retry. The pull-up is restored
 *       because device deinitialization can clear its GPIO configuration.
 */
static esp_err_t sd_cleanup_locked(void)
{
    esp_err_t result = ESP_OK;
    sd_card = NULL;
    if (sd_fat_registered)
    {
        if (f_mount(NULL, sd_fat_drive, 0U) == FR_OK)
        {
            sd_fat_registered = false;
        }
        else
        {
            result = ESP_FAIL;
        }
    }
    if (!sd_fat_registered)
    {
        if (sd_vfs_registered)
        {
            result = esp_vfs_fat_unregister_path(sd_mount_path);
            /* The pinned VFS implementation destroys its context even on unregister errors. */
            sd_vfs_registered = false;
            sd_filesystem = NULL;
        }
        if (sd_disk_registered)
        {
            ff_diskio_unregister(sd_drive);
            sd_disk_registered = false;
        }
        sd_drive = FF_DRV_NOT_USED;
        if (sd_device_initialized)
        {
            const esp_err_t device_result = sdspi_host_remove_device(sd_device);
            if (device_result == ESP_OK)
            {
                sd_device_initialized = false;
            }
            if (result == ESP_OK)
            {
                result = device_result;
            }
        }
        if (!sd_device_initialized && sd_bus_initialized)
        {
            const esp_err_t bus_result = spi_bus_free(SPI2_HOST);
            if (bus_result == ESP_OK)
            {
                sd_bus_initialized = false;
            }
            if (result == ESP_OK)
            {
                result = bus_result;
            }
        }
    }
    /* SDSPI deinit clears the CD pull-up along with its device GPIO configuration. */
    const esp_err_t detect_result = sd_configure_detect();
    if (result == ESP_OK)
    {
        result = detect_result;
    }
    return result;
}

/**
 * @brief Validate card presence and clean up a removed card before an operation.
 * @return ESP_OK when mounted, NOT_FOUND when absent, or a state/cleanup error.
 * @pre Caller holds the SD mutex.
 */
static esp_err_t sd_require_mounted_locked(void)
{
    esp_err_t result = ESP_OK;
    if (!sd_present())
    {
        result = sd_cleanup_locked();
        if (result == ESP_OK)
        {
            result = ESP_ERR_NOT_FOUND;
        }
    }
    else if (sd_card == NULL)
    {
        result = ESP_ERR_INVALID_STATE;
    }
    return result;
}

/**
 * @brief Recheck removal, record the operation result and release the SD mutex.
 * @param[in] result Result from the completed operation.
 * @return Final operation or removal/cleanup error.
 * @pre All file and directory handles are closed and the caller holds the SD mutex.
 */
static esp_err_t sd_finish_locked(esp_err_t result)
{
    if (!sd_present())
    {
        const esp_err_t cleanup_result = sd_cleanup_locked();
        result = (cleanup_result == ESP_OK) ? ESP_ERR_NOT_FOUND : cleanup_result;
    }
    sd_last_error = result;
    (void)xSemaphoreGive(sd_mutex);
    return result;
}

/**
 * @brief Acquire SPI2 and its SDSPI device, then initialize the static card descriptor.
 */
static esp_err_t sd_initialize_card_locked(void)
{
    const spi_bus_config_t bus = {.mosi_io_num = CONFIG_PROBE_SD_MOSI_GPIO,
                                  .miso_io_num = CONFIG_PROBE_SD_MISO_GPIO,
                                  .sclk_io_num = CONFIG_PROBE_SD_SCK_GPIO,
                                  .quadwp_io_num = -1,
                                  .quadhd_io_num = -1,
                                  .data4_io_num = -1,
                                  .data5_io_num = -1,
                                  .data6_io_num = -1,
                                  .data7_io_num = -1,
                                  .max_transfer_sz = 4096};
    esp_err_t result = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (result == ESP_OK)
    {
        sd_bus_initialized = true;
        result = sdspi_host_init();
        if (result == ESP_OK)
        {
            sdspi_device_config_t device = SDSPI_DEVICE_CONFIG_DEFAULT();
            device.host_id = SPI2_HOST;
            device.gpio_cs = (gpio_num_t)CONFIG_PROBE_SD_CS_GPIO;
            device.gpio_cd = (gpio_num_t)CONFIG_PROBE_SD_DETECT_GPIO;
            result = sdspi_host_init_device(&device, &sd_device);
            if (result == ESP_OK)
            {
                sd_device_initialized = true;
                sdmmc_host_t host = SDSPI_HOST_DEFAULT();
                host.slot = sd_device;
                host.max_freq_khz = CONFIG_PROBE_SD_MAX_FREQ_KHZ;
                host.command_timeout_ms = 1000;
                (void)memset(&sd_card_storage, 0, sizeof(sd_card_storage));
                result = sdmmc_card_init(&host, &sd_card_storage);
            }
        }
    }
    return result;
}

/**
 * @brief Register disk and VFS ownership before mounting an existing FAT filesystem.
 */
static esp_err_t sd_register_fat_locked(void)
{
    esp_err_t result = ff_diskio_get_drive(&sd_drive);
    if ((result == ESP_OK) && ((sd_drive >= FF_VOLUMES) || (sd_drive >= 10U)))
    {
        result = ESP_ERR_INVALID_SIZE;
    }
    if (result == ESP_OK)
    {
        sd_fat_drive[0] = (char)('0' + (int)sd_drive);
        sd_fat_drive[1] = ':';
        sd_fat_drive[2] = '\0';
        /* The SDK's void registration API owns its internal allocation/error policy. */
        ff_diskio_register_sdmmc(sd_drive, &sd_card_storage);
        sd_disk_registered = true;
        ff_sdmmc_set_disk_status_check(sd_drive, true);
        const esp_vfs_fat_conf_t configuration = {
            .base_path = sd_mount_path, .fat_drive = sd_fat_drive, .max_files = 2U};
        result = esp_vfs_fat_register(&configuration, &sd_filesystem);
        if (result == ESP_OK)
        {
            sd_vfs_registered = true;
            /* Failed immediate mounts can still register the FATFS pointer internally. */
            sd_fat_registered = true;
            const FRESULT mount_result = f_mount(sd_filesystem, sd_fat_drive, 1U);
            if (mount_result == FR_OK)
            {
                sd_card = &sd_card_storage;
            }
            else
            {
                result = (mount_result == FR_NOT_ENOUGH_CORE) ? ESP_ERR_NO_MEM : ESP_FAIL;
            }
        }
        else
        {
            /* INVALID_STATE may return somebody else's FATFS; never adopt or unregister it. */
            sd_filesystem = NULL;
        }
    }
    return result;
}

/**
 * @brief Keep a valid mount or rebuild the mount stages after previous cleanup.
 * @return ESP_OK when mounted, NOT_FOUND when absent, or an SDK/cleanup error.
 * @pre Caller holds the SD mutex. A failed stage is unwound without formatting media.
 */
static esp_err_t sd_mount_locked(void)
{
    esp_err_t result = ESP_OK;
    if (!sd_present())
    {
        result = sd_cleanup_locked();
        if (result == ESP_OK)
        {
            result = ESP_ERR_NOT_FOUND;
        }
    }
    else if (sd_card == NULL)
    {
        if (sd_resources_owned())
        {
            result = sd_cleanup_locked();
        }
        if (result == ESP_OK)
        {
            result = sd_initialize_card_locked();
            if (result == ESP_OK)
            {
                result = sd_register_fat_locked();
            }
            if (result != ESP_OK)
            {
                const esp_err_t cleanup_result = sd_cleanup_locked();
                if (cleanup_result != ESP_OK)
                {
                    result = cleanup_result;
                }
            }
        }
    }
    return result;
}

/**
 * @brief Validate one portable ASCII 8.3 basename without paths or aliases.
 * @param[in] name NUL-terminated candidate; NULL is rejected.
 * @return true for a 1..8 character stem and optional 1..3 character extension.
 */
static bool sd_name_valid(const char *name)
{
    bool extension = false;
    size_t stem_length = 0U;
    size_t extension_length = 0U;
    bool valid = false;
    if (name != NULL)
    {
        for (size_t index = 0U; index < PROBE_SD_NAME_CAPACITY; ++index)
        {
            const char value = name[index];
            if (value == '\0')
            {
                valid = (stem_length > 0U) && (!extension || (extension_length > 0U));
                break;
            }
            if ((value == '.') && !extension && (stem_length > 0U))
            {
                extension = true;
            }
            else if (((value >= 'A') && (value <= 'Z')) || ((value >= 'a') && (value <= 'z')) ||
                     ((value >= '0') && (value <= '9')) || (value == '_') || (value == '-'))
            {
                if (extension)
                {
                    ++extension_length;
                }
                else
                {
                    ++stem_length;
                }
                if ((stem_length > 8U) || (extension_length > 3U))
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
    }
    return valid;
}

/**
 * @brief Join the mount point and a validated basename without truncation.
 * @param[in] name Candidate basename using sd_name_valid rules.
 * @param[out] path Non-NULL caller-owned output buffer.
 * @param[in] capacity Output capacity in bytes, including the terminator.
 * @return ESP_OK when the complete path fits, or ESP_ERR_INVALID_ARG.
 */
static esp_err_t sd_make_path(const char *name, char *path, size_t capacity)
{
    esp_err_t result = ESP_ERR_INVALID_ARG;
    if (sd_name_valid(name))
    {
        const int length = snprintf(path, capacity, "%s/%s", sd_mount_path, name);
        if ((length > 0) && ((size_t)length < capacity))
        {
            result = ESP_OK;
        }
    }
    return result;
}

/**
 * @brief Translate selected errno values into the SD public error vocabulary.
 * @param[in] error Saved errno from the failed filesystem operation.
 * @return A matching ESP_ERR_* value, or ESP_FAIL for other errors.
 */
static esp_err_t sd_io_error(int error)
{
    esp_err_t result = ESP_FAIL;
    if (error == ENOENT)
    {
        result = ESP_ERR_NOT_FOUND;
    }
    else if (error == EEXIST)
    {
        result = ESP_ERR_INVALID_STATE;
    }
    else if (error == ENOMEM)
    {
        result = ESP_ERR_NO_MEM;
    }
    else if (error == EINVAL)
    {
        result = ESP_ERR_INVALID_ARG;
    }
    return result;
}

/**
 * @brief Scan at most 32 directory records and return only complete supported names.
 * @param[out] entries Caller-owned array of at least capacity entries.
 * @param[in] capacity Maximum entries to publish.
 * @param[out] count Entry count, written on success.
 * @param[out] truncated True when entries were skipped or the scan did not exhaust the root.
 * @return ESP_OK after closing the directory, or a translated filesystem error.
 * @pre Caller holds the SD mutex and has validated the mount and output buffers.
 */
static esp_err_t sd_list_locked(probe_sd_entry_t *entries, size_t capacity, size_t *count,
                                bool *truncated)
{
    DIR *const directory = opendir(sd_mount_path);
    esp_err_t result = (directory != NULL) ? ESP_OK : sd_io_error(errno);
    size_t found = 0U;
    bool omitted = false;
    bool exhausted = false;
    if (directory != NULL)
    {
        for (size_t record = 0U; (record < 32U) && (result == ESP_OK); ++record)
        {
            errno = 0;
            const struct dirent *const entry = readdir(directory);
            if (entry == NULL)
            {
                exhausted = true;
                if (errno != 0)
                {
                    result = sd_io_error(errno);
                }
                break;
            }
            if ((strcmp(entry->d_name, ".") != 0) && (strcmp(entry->d_name, "..") != 0))
            {
                if (!sd_name_valid(entry->d_name))
                {
                    omitted = true;
                }
                else if (found == capacity)
                {
                    omitted = true;
                    break;
                }
                else
                {
                    char path[sizeof(sd_mount_path) + PROBE_SD_NAME_CAPACITY] = {0};
                    result = sd_make_path(entry->d_name, path, sizeof(path));
                    struct stat information = {0};
                    if ((result == ESP_OK) && (stat(path, &information) != 0))
                    {
                        result = sd_io_error(errno);
                    }
                    if ((result == ESP_OK) && (information.st_size < 0))
                    {
                        /* The target's signed 32-bit off_t cannot describe this entry. */
                        omitted = true;
                    }
                    else if (result == ESP_OK)
                    {
                        (void)memcpy(entries[found].name, entry->d_name,
                                     strlen(entry->d_name) + 1U);
                        entries[found].directory = S_ISDIR(information.st_mode);
                        entries[found].size_bytes =
                            (information.st_size > 0) ? (uint64_t)information.st_size : UINT64_C(0);
                        ++found;
                    }
                }
            }
        }
        if ((closedir(directory) != 0) && (result == ESP_OK))
        {
            result = sd_io_error(errno);
        }
    }
    if (result == ESP_OK)
    {
        *count = found;
        *truncated = omitted || !exhausted;
    }
    return result;
}
#endif

/**
 * @brief Create serialization and card detection, then attempt the initial optional FAT mount.
 */
esp_err_t probe_sd_start(void)
{
    esp_err_t result = ESP_ERR_NOT_SUPPORTED;
#if CONFIG_PROBE_SD_ENABLE
    if (sd_mutex != NULL)
    {
        result = ESP_ERR_INVALID_STATE;
    }
    else
    {
        sd_mutex = xSemaphoreCreateMutex();
        result = (sd_mutex != NULL) ? sd_configure_detect() : ESP_ERR_NO_MEM;
        if (result == ESP_OK)
        {
            result = probe_sd_mount();
        }
        else if (sd_mutex != NULL)
        {
            vSemaphoreDelete(sd_mutex);
            sd_mutex = NULL;
        }
    }
#endif
    return result;
}

/**
 * @brief Serialize a mount attempt and publish the final result after checking removal.
 */
esp_err_t probe_sd_mount(void)
{
    esp_err_t result = ESP_ERR_NOT_SUPPORTED;
#if CONFIG_PROBE_SD_ENABLE
    result = sd_lock();
    if (result == ESP_OK)
    {
        result = sd_finish_locked(sd_mount_locked());
    }
#endif
    return result;
}

/**
 * @brief Serialize cleanup and retain ownership flags needed for a failed-stage retry.
 */
esp_err_t probe_sd_unmount(void)
{
    esp_err_t result = ESP_ERR_NOT_SUPPORTED;
#if CONFIG_PROBE_SD_ENABLE
    result = sd_lock();
    if (result == ESP_OK)
    {
        result = sd_cleanup_locked();
        sd_last_error = result;
        (void)xSemaphoreGive(sd_mutex);
    }
#endif
    return result;
}

/**
 * @brief Snapshot detect state, capacity and mount ownership after stale-card cleanup.
 */
esp_err_t probe_sd_get_status(probe_sd_status_t *status)
{
    esp_err_t result = ESP_ERR_NOT_SUPPORTED;
#if CONFIG_PROBE_SD_ENABLE
    result = (status != NULL) ? sd_lock() : ESP_ERR_INVALID_ARG;
    if (result == ESP_OK)
    {
        const bool present = sd_present();
        if (!present && sd_resources_owned())
        {
            const esp_err_t cleanup_result = sd_cleanup_locked();
            sd_last_error = (cleanup_result == ESP_OK) ? ESP_ERR_NOT_FOUND : cleanup_result;
        }
        uint64_t capacity = UINT64_C(0);
        if ((sd_card != NULL) && (sd_card->csd.capacity > 0) && (sd_card->csd.sector_size > 0))
        {
            capacity = (uint64_t)sd_card->csd.capacity * (uint64_t)sd_card->csd.sector_size;
        }
        *status = (probe_sd_status_t){.enabled = true,
                                      .present = present,
                                      .mounted = sd_card != NULL,
                                      .capacity_bytes = capacity,
                                      .last_error = sd_last_error};
        (void)xSemaphoreGive(sd_mutex);
    }
#else
    (void)status;
#endif
    return result;
}

/**
 * @brief Validate output bounds and serialize a bounded scan of root directory entries.
 */
esp_err_t probe_sd_list(probe_sd_entry_t *entries, size_t capacity, size_t *count, bool *truncated)
{
    esp_err_t result = ESP_ERR_NOT_SUPPORTED;
    if (count != NULL)
    {
        *count = 0U;
    }
    if (truncated != NULL)
    {
        *truncated = false;
    }
#if CONFIG_PROBE_SD_ENABLE
    if ((entries == NULL) || (capacity == 0U) || (capacity > PROBE_SD_MAX_LIST_ENTRIES) ||
        (count == NULL) || (truncated == NULL))
    {
        result = ESP_ERR_INVALID_ARG;
    }
    else
    {
        result = sd_lock();
        if (result == ESP_OK)
        {
            result = sd_require_mounted_locked();
            if (result == ESP_OK)
            {
                result = sd_list_locked(entries, capacity, count, truncated);
            }
            result = sd_finish_locked(result);
            if (result != ESP_OK)
            {
                *count = 0U;
                *truncated = false;
            }
        }
    }
#else
    (void)entries;
    (void)capacity;
#endif
    return result;
}

/**
 * @brief Read a bounded file prefix and close its descriptor before checking card removal.
 */
esp_err_t probe_sd_read(const char *name, void *data, size_t capacity, size_t *length,
                        bool *truncated)
{
    esp_err_t result = ESP_ERR_NOT_SUPPORTED;
    if (length != NULL)
    {
        *length = 0U;
    }
    if (truncated != NULL)
    {
        *truncated = false;
    }
#if CONFIG_PROBE_SD_ENABLE
    char path[sizeof(sd_mount_path) + PROBE_SD_NAME_CAPACITY] = {0};
    if ((data == NULL) || (capacity == 0U) || (capacity > PROBE_SD_MAX_IO_BYTES) ||
        (length == NULL) || (truncated == NULL))
    {
        result = ESP_ERR_INVALID_ARG;
    }
    else
    {
        result = sd_make_path(name, path, sizeof(path));
        if (result == ESP_OK)
        {
            result = sd_lock();
            if (result == ESP_OK)
            {
                result = sd_require_mounted_locked();
                if (result == ESP_OK)
                {
                    const int file = open(path, O_RDONLY);
                    if (file < 0)
                    {
                        result = sd_io_error(errno);
                    }
                    else
                    {
                        struct stat information = {0};
                        if (fstat(file, &information) != 0)
                        {
                            result = sd_io_error(errno);
                        }
                        else if (!S_ISREG(information.st_mode))
                        {
                            result = ESP_ERR_INVALID_ARG;
                        }
                        else if (information.st_size < 0)
                        {
                            result = ESP_ERR_NOT_SUPPORTED;
                        }
                        else
                        {
                            const ssize_t received = read(file, data, capacity);
                            if (received < 0)
                            {
                                result = sd_io_error(errno);
                            }
                            else
                            {
                                *length = (size_t)received;
                                *truncated = (uint64_t)information.st_size > (uint64_t)*length;
                            }
                        }
                        if ((close(file) != 0) && (result == ESP_OK))
                        {
                            result = sd_io_error(errno);
                        }
                    }
                }
                result = sd_finish_locked(result);
                if (result != ESP_OK)
                {
                    *length = 0U;
                    *truncated = false;
                }
            }
        }
    }
#else
    (void)name;
    (void)data;
    (void)capacity;
#endif
    return result;
}

/**
 * @brief Create a new root file exclusively, synchronize it and close before unlocking.
 */
esp_err_t probe_sd_write(const char *name, const void *data, size_t length)
{
    esp_err_t result = ESP_ERR_NOT_SUPPORTED;
#if CONFIG_PROBE_SD_ENABLE
    char path[sizeof(sd_mount_path) + PROBE_SD_NAME_CAPACITY] = {0};
    if (((data == NULL) && (length != 0U)) || (length > PROBE_SD_MAX_IO_BYTES))
    {
        result = ESP_ERR_INVALID_ARG;
    }
    else
    {
        result = sd_make_path(name, path, sizeof(path));
        if (result == ESP_OK)
        {
            result = sd_lock();
            if (result == ESP_OK)
            {
                result = sd_require_mounted_locked();
                if (result == ESP_OK)
                {
                    const int file = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
                    if (file < 0)
                    {
                        result = sd_io_error(errno);
                    }
                    else
                    {
                        if (length != 0U)
                        {
                            const ssize_t written = write(file, data, length);
                            if (written < 0)
                            {
                                result = sd_io_error(errno);
                            }
                            else if ((size_t)written != length)
                            {
                                result = ESP_FAIL;
                            }
                        }
                        if ((fsync(file) != 0) && (result == ESP_OK))
                        {
                            result = sd_io_error(errno);
                        }
                        if ((close(file) != 0) && (result == ESP_OK))
                        {
                            result = sd_io_error(errno);
                        }
                    }
                }
                result = sd_finish_locked(result);
            }
        }
    }
#else
    (void)name;
    (void)data;
    (void)length;
#endif
    return result;
}
