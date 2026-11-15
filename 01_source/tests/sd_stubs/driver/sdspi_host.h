/* SPDX-License-Identifier: GPL-3.0-or-later */
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
esp_err_t sdspi_host_init(void);
esp_err_t sdspi_host_init_device(const sdspi_device_config_t *device, sdspi_dev_handle_t *handle);
esp_err_t sdspi_host_remove_device(sdspi_dev_handle_t handle);
#endif
