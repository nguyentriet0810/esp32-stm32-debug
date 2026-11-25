/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file esp_lcd_panel_io.h @brief OLED I2C framing and transfer interface for host tests. */
#ifndef OLED_TEST_PANEL_IO_H
#define OLED_TEST_PANEL_IO_H
#include <stddef.h>
#include <stdint.h>
#include "driver/i2c_master.h"
typedef struct oled_test_io *esp_lcd_panel_io_handle_t;
typedef struct
{
    uint32_t dev_addr;
    uint32_t scl_speed_hz;
    size_t control_phase_bytes;
    uint8_t dc_bit_offset;
    int lcd_cmd_bits;
    int lcd_param_bits;
    int transaction_timeout_ms;
} esp_lcd_panel_io_i2c_config_t;
/** @brief Validate command/data framing and acquire the test I/O token. */
esp_err_t esp_lcd_new_panel_io_i2c(i2c_master_bus_handle_t bus,
                                   const esp_lcd_panel_io_i2c_config_t *config,
                                   esp_lcd_panel_io_handle_t *io);
/** @brief Release the test-owned panel-I/O token. */
esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t io);
/** @brief Decode the transmitted command stream using the SH1106 command vocabulary. */
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t io, int command,
                                    const void *parameters, size_t size);
/** @brief Write the current page with SH1106 column-only auto-increment semantics. */
esp_err_t esp_lcd_panel_io_tx_color(esp_lcd_panel_io_handle_t io, int command, const void *data,
                                    size_t size);
#endif
