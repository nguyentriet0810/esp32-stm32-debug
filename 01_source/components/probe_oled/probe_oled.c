/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_oled.c @brief Compact 5x7 renderer using the SDK SSD1306 panel driver. */
#include "probe_oled.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "sdkconfig.h"
#if CONFIG_PROBE_OLED_ENABLE
static i2c_master_bus_handle_t oled_bus;
#endif
static esp_lcd_panel_io_handle_t oled_io;
static esp_lcd_panel_handle_t oled_panel;
static uint8_t frame[1024];
typedef struct
{
    char character;
    uint8_t columns[5];
} glyph_t;
/* Original compact glyph definitions. Each column is LSB-first from top to bottom. */
static const glyph_t glyphs[] = {{'0', {0x3e, 0x51, 0x49, 0x45, 0x3e}},
                                 {'1', {0, 0x42, 0x7f, 0x40, 0}},
                                 {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
                                 {'3', {0x21, 0x41, 0x45, 0x4b, 0x31}},
                                 {'4', {0x18, 0x14, 0x12, 0x7f, 0x10}},
                                 {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
                                 {'6', {0x3c, 0x4a, 0x49, 0x49, 0x30}},
                                 {'7', {1, 0x71, 9, 5, 3}},
                                 {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
                                 {'9', {6, 0x49, 0x49, 0x29, 0x1e}},
                                 {'A', {0x7e, 0x11, 0x11, 0x11, 0x7e}},
                                 {'B', {0x7f, 0x49, 0x49, 0x49, 0x36}},
                                 {'C', {0x3e, 0x41, 0x41, 0x41, 0x22}},
                                 {'D', {0x7f, 0x41, 0x41, 0x22, 0x1c}},
                                 {'E', {0x7f, 0x49, 0x49, 0x49, 0x41}},
                                 {'F', {0x7f, 9, 9, 9, 1}},
                                 {'G', {0x3e, 0x41, 0x49, 0x49, 0x7a}},
                                 {'H', {0x7f, 8, 8, 8, 0x7f}},
                                 {'I', {0, 0x41, 0x7f, 0x41, 0}},
                                 {'J', {0x20, 0x40, 0x41, 0x3f, 1}},
                                 {'K', {0x7f, 8, 0x14, 0x22, 0x41}},
                                 {'L', {0x7f, 0x40, 0x40, 0x40, 0x40}},
                                 {'M', {0x7f, 2, 0x0c, 2, 0x7f}},
                                 {'N', {0x7f, 4, 8, 0x10, 0x7f}},
                                 {'O', {0x3e, 0x41, 0x41, 0x41, 0x3e}},
                                 {'P', {0x7f, 9, 9, 9, 6}},
                                 {'Q', {0x3e, 0x41, 0x51, 0x21, 0x5e}},
                                 {'R', {0x7f, 9, 0x19, 0x29, 0x46}},
                                 {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
                                 {'T', {1, 1, 0x7f, 1, 1}},
                                 {'U', {0x3f, 0x40, 0x40, 0x40, 0x3f}},
                                 {'V', {0x1f, 0x20, 0x40, 0x20, 0x1f}},
                                 {'W', {0x3f, 0x40, 0x38, 0x40, 0x3f}},
                                 {'X', {0x63, 0x14, 8, 0x14, 0x63}},
                                 {'Y', {7, 8, 0x70, 8, 7}},
                                 {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
                                 {':', {0, 0x36, 0x36, 0, 0}},
                                 {'.', {0, 0x60, 0x60, 0, 0}},
                                 {'-', {8, 8, 8, 8, 8}},
                                 {'/', {0x20, 0x10, 8, 4, 2}},
                                 {'>', {0, 0x41, 0x22, 0x14, 8}},
                                 {'<', {8, 0x14, 0x22, 0x41, 0}},
                                 {'?', {2, 1, 0x51, 9, 6}},
                                 {' ', {0, 0, 0, 0, 0}}};
#if CONFIG_PROBE_OLED_ENABLE
static void oled_release(void)
{
    if (oled_panel != NULL)
    {
        (void)esp_lcd_panel_del(oled_panel);
        oled_panel = NULL;
    }
    if (oled_io != NULL)
    {
        (void)esp_lcd_panel_io_del(oled_io);
        oled_io = NULL;
    }
    if (oled_bus != NULL)
    {
        (void)i2c_del_master_bus(oled_bus);
        oled_bus = NULL;
    }
}
#endif
esp_err_t probe_oled_init(void)
{
    esp_err_t result = ESP_ERR_NOT_SUPPORTED;
#if CONFIG_PROBE_OLED_ENABLE
    const i2c_master_bus_config_t bus = {.i2c_port = I2C_NUM_0,
                                         .sda_io_num = CONFIG_PROBE_OLED_SDA_GPIO,
                                         .scl_io_num = CONFIG_PROBE_OLED_SCL_GPIO,
                                         .clk_source = I2C_CLK_SRC_DEFAULT,
                                         .glitch_ignore_cnt = 7U,
                                         .flags.enable_internal_pullup = true};
    result = i2c_new_master_bus(&bus, &oled_bus);
    uint32_t address = 0x3cU;
    if (result == ESP_OK)
    {
        result = i2c_master_probe(oled_bus, (uint16_t)address, 30);
        if (result != ESP_OK)
        {
            address = 0x3dU;
            result = i2c_master_probe(oled_bus, (uint16_t)address, 30);
        }
    }
    if (result == ESP_OK)
    {
        const esp_lcd_panel_io_i2c_config_t io = {.dev_addr = address,
                                                  .scl_speed_hz = 100000U,
                                                  .transaction_timeout_ms = 250U,
                                                  .control_phase_bytes = 1U,
                                                  .lcd_cmd_bits = 8U,
                                                  .lcd_param_bits = 8U,
                                                  .dc_bit_offset = 6U};
        result = esp_lcd_new_panel_io_i2c(oled_bus, &io, &oled_io);
    }
    if (result == ESP_OK)
    {
        esp_lcd_panel_ssd1306_config_t model = {.height = 64U};
        const esp_lcd_panel_dev_config_t panel = {
            .reset_gpio_num = -1, .bits_per_pixel = 1U, .vendor_config = &model};
        result = esp_lcd_new_panel_ssd1306(oled_io, &panel, &oled_panel);
    }
    if (result == ESP_OK)
    {
        result = esp_lcd_panel_reset(oled_panel);
    }
    if (result == ESP_OK)
    {
        result = esp_lcd_panel_init(oled_panel);
    }
    if (result == ESP_OK)
    {
        result = esp_lcd_panel_disp_on_off(oled_panel, true);
    }
    if (result != ESP_OK)
    {
        oled_release();
        result = ESP_ERR_NOT_FOUND;
    }
#endif
    return result;
}
esp_err_t probe_oled_draw(const char lines[][22], size_t count)
{
    esp_err_t result = ESP_ERR_INVALID_STATE;
    if ((oled_panel != NULL) && (lines != NULL) && (count <= 8U))
    {
        (void)memset(frame, 0, sizeof(frame));
        for (size_t row = 0U; row < count; row++)
        {
            for (size_t column = 0U; (column < 21U) && (lines[row][column] != '\0'); column++)
            {
                char character = lines[row][column];
                if ((character >= 'a') && (character <= 'z'))
                {
                    character = (char)(character - 'a' + 'A');
                }
                for (size_t glyph = 0U; glyph < (sizeof(glyphs) / sizeof(glyphs[0])); glyph++)
                {
                    if (glyphs[glyph].character == character)
                    {
                        (void)memcpy(&frame[(row * 128U) + (column * 6U)], glyphs[glyph].columns,
                                     5U);
                        break;
                    }
                }
            }
        }
        result = esp_lcd_panel_draw_bitmap(oled_panel, 0, 0, 128, 64, frame);
    }
    return result;
}
esp_err_t probe_oled_contrast(unsigned int value)
{
    esp_err_t result = ESP_ERR_INVALID_ARG;
    if ((oled_io != NULL) && (value <= 255U))
    {
        const uint8_t contrast = (uint8_t)value;
        result = esp_lcd_panel_io_tx_param(oled_io, 0x81, &contrast, 1U);
    }
    return result;
}
