#include <inttypes.h>

#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_psram.h"

static const char *TAG = "probe_board";

static esp_err_t check_board(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI(TAG, "ESP-IDF %s; chip revision %u; cores %u",
             esp_get_idf_version(), chip.revision, chip.cores);
    if (chip.model != CHIP_ESP32S3 || chip.cores != 2) {
        ESP_LOGE(TAG, "Expected a dual-core ESP32-S3");
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint32_t physical_flash = 0;
    uint32_t configured_flash = 0;
    esp_err_t err = esp_flash_get_physical_size(NULL, &physical_flash);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_flash_get_size(NULL, &configured_flash);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "Flash: physical %" PRIu32 " bytes, configured %" PRIu32 " bytes",
             physical_flash, configured_flash);
    if (physical_flash != 16U * 1024U * 1024U || configured_flash != physical_flash) {
        ESP_LOGE(TAG, "Expected 16 MB flash for the N16R8 profile");
        return ESP_ERR_INVALID_SIZE;
    }

    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM is not initialized; this profile requires Octal PSRAM");
        return ESP_ERR_INVALID_STATE;
    }
    const size_t psram_size = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM: %zu bytes", psram_size);
    if (psram_size != 8U * 1024U * 1024U) {
        ESP_LOGE(TAG, "Expected 8 MB PSRAM for the N16R8 profile");
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "Free internal heap: %zu bytes",
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    ESP_LOGI(TAG, "Free PSRAM heap: %zu bytes",
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    return ESP_OK;
}

void app_main(void)
{
    ESP_ERROR_CHECK(check_board());
    ESP_LOGI(TAG, "Board bring-up passed: ESP32-S3 N16R8");
    ESP_LOGI(TAG, "SWD and network services will be added in the next implementation stage");
}
