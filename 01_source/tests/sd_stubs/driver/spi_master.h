/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file spi_master.h
 * @brief SPI bus ownership test interface.
 *
 * Defines the SPI2 bus configuration subset validated by the SD test harness. Test-only header;
 * not used in firmware builds.
 */
#ifndef SD_TEST_SPI_H
#define SD_TEST_SPI_H
#include "esp_err.h"
typedef int spi_host_device_t;
#define SPI2_HOST 1
#define SPI_DMA_CH_AUTO 3
typedef struct
{
    int mosi_io_num;
    int miso_io_num;
    int sclk_io_num;
    int quadwp_io_num;
    int quadhd_io_num;
    int data4_io_num;
    int data5_io_num;
    int data6_io_num;
    int data7_io_num;
    int max_transfer_sz;
} spi_bus_config_t;
/**
 * @brief Assert PCB SPI2 pins and exclusive bus acquisition before recording ownership.
 */
esp_err_t spi_bus_initialize(spi_host_device_t host, const spi_bus_config_t *config, int dma);
/**
 * @brief Require device teardown before releasing SPI2, with an optional one-shot failure.
 */
esp_err_t spi_bus_free(spi_host_device_t host);
#endif
