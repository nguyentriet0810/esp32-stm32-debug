/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file i2c_master.h @brief Minimal synchronous I2C bus API used by the OLED host harness. */
#ifndef OLED_TEST_I2C_MASTER_H
#define OLED_TEST_I2C_MASTER_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct oled_test_bus *i2c_master_bus_handle_t;
typedef struct
{
    int i2c_port;
    int sda_io_num;
    int scl_io_num;
    int clk_source;
    unsigned int glitch_ignore_cnt;
    struct
    {
        bool enable_internal_pullup;
    } flags;
} i2c_master_bus_config_t;
#define I2C_NUM_0 0
#define I2C_CLK_SRC_DEFAULT 0
/** @brief Validate the PCB configuration and allocate a test-owned bus token. */
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *config, i2c_master_bus_handle_t *bus);
/** @brief Release the test bus only after the display I/O token is released. */
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus);
/** @brief Return ACK only for the address selected by the current fixture. */
esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus, uint16_t address, int timeout_ms);
#endif
