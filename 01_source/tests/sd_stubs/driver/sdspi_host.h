/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file sdspi_host.h
 * @brief SDSPI device test interface.
 *
 * Models the device configuration and handle lifecycle consumed by the production SD component.
 * Test-only header; not used in firmware builds.
 */
#ifndef SD_TEST_SDSPI_H
#define SD_TEST_SDSPI_H
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "sdmmc_cmd.h"
typedef struct
{
    spi_host_device_t host_id;
    gpio_num_t gpio_cs;
    gpio_num_t gpio_cd;
    gpio_num_t gpio_wp;
} sdspi_device_config_t;
#define SDSPI_HOST_DEFAULT() {.slot = SPI2_HOST, .max_freq_khz = 20000, .command_timeout_ms = 0}
#define SDSPI_DEVICE_CONFIG_DEFAULT()                                                              \
    {.host_id = SPI2_HOST, .gpio_cs = 13, .gpio_cd = -1, .gpio_wp = -1}
#define SDSPI_DEFAULT_DMA SPI_DMA_CH_AUTO
typedef int sdspi_dev_handle_t;
/**
 * @brief Provide a successful no-allocation SDSPI host initialization test double.
 */
esp_err_t sdspi_host_init(void);
/**
 * @brief Validate device pins and transfer one simulated SDSPI handle to the caller.
 */
esp_err_t sdspi_host_init_device(const sdspi_device_config_t *device, sdspi_dev_handle_t *handle);
/**
 * @brief Verify reverse cleanup order and model the SDK clearing the detect pull-up.
 */
esp_err_t sdspi_host_remove_device(sdspi_dev_handle_t handle);
#endif
