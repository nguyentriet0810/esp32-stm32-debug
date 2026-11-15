/* SPDX-License-Identifier: GPL-3.0-or-later */
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
esp_err_t spi_bus_initialize(spi_host_device_t host, const spi_bus_config_t *config, int dma);
esp_err_t spi_bus_free(spi_host_device_t host);
#endif
