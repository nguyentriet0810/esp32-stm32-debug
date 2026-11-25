/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_oled.c
 * @brief SH1106 page-addressed display lifecycle and text rendering.
 *
 * A single UI task owns the I2C bus, panel I/O and fixed framebuffer. The 128x64 image is
 * written into the controller's 132-column RAM one eight-row page at a time.
 */
#include "probe_oled.h"
#include "sdkconfig.h"

#if CONFIG_PROBE_OLED_ENABLE
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "font8x8_basic_tr.h"

#define OLED_WIDTH 128U
#define OLED_PAGES 8U
#define OLED_HEIGHT (OLED_PAGES * 8U)
#define SH1106_RAM_COLUMNS 132U

_Static_assert(CONFIG_PROBE_OLED_COLUMN_OFFSET >= 0 &&
                   CONFIG_PROBE_OLED_COLUMN_OFFSET <= (SH1106_RAM_COLUMNS - OLED_WIDTH),
               "SH1106 visible image must fit within its 132 RAM columns");

static i2c_master_bus_handle_t oled_bus;
static esp_lcd_panel_io_handle_t oled_io;
static bool oled_ready;
static uint8_t frame[OLED_WIDTH * OLED_PAGES];
/* The UI task owns both buffers; the wordmark mask is built once without heap allocation. */
static uint8_t startup_logo[OLED_WIDTH * OLED_PAGES];
static bool startup_logo_ready;
/**
 * @brief Look up a column-wise glyph shared by the menu and startup wordmark.
 * @param[in] character 7-bit ASCII or control-code arrow; high bytes are unsupported.
 * @return Borrowed eight-column bitmap, or NULL for an unsupported code point.
 */
static const uint8_t *oled_glyph(char character)
{
    const uint8_t code = (uint8_t)(unsigned char)character;
    return (code < 128U) ? font8x8_basic_tr[code] : NULL;
}

/**
 * @brief Set one logical image pixel in the SH1106 eight-row-page framebuffer layout.
 * @param[in,out] pixels Caller-owned 1024-byte image.
 * @param[in] x Logical column, 0..127, in the current readable orientation.
 * @param[in] y Logical row, 0..63, in the current readable orientation.
 */
static void oled_pixel(uint8_t *pixels, size_t x, size_t y)
{
    pixels[((y / 8U) * OLED_WIDTH) + x] |= (uint8_t)(1U << (y % 8U));
}

/** @brief Cache three centered lines using doubled 8x8 cells; DEBUGGER fits all 128 columns. */
static void oled_build_startup_logo(void)
{
    static const char *const words[] = {"MAGIC", "DEBUGGER", "TOOL"};
    if (!startup_logo_ready)
    {
        (void)memset(startup_logo, 0, sizeof(startup_logo));
        for (size_t line = 0U; line < 3U; ++line)
        {
            const size_t length = strlen(words[line]);
            const size_t width = length * 8U * 2U;
            const size_t left = (OLED_WIDTH - width) / 2U;
            const size_t top = 5U + (line * 20U);
            for (size_t letter = 0U; letter < length; ++letter)
            {
                const uint8_t *const glyph = oled_glyph(words[line][letter]);
                if (glyph != NULL)
                {
                    for (size_t column = 0U; column < 8U; ++column)
                    {
                        for (size_t row = 0U; row < 8U; ++row)
                        {
                            if ((glyph[column] & (uint8_t)(1U << row)) != 0U)
                            {
                                for (size_t dy = 0U; dy < 2U; ++dy)
                                {
                                    for (size_t dx = 0U; dx < 2U; ++dx)
                                    {
                                        oled_pixel(startup_logo,
                                                   left + (letter * 16U) + (column * 2U) + dx,
                                                   top + (row * 2U) + dy);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        startup_logo_ready = true;
    }
}

/**
 * @brief Evaluate a monotonic 45-degree sweep with a sixteen-pixel ordered-dither edge.
 * @param[in] x Logical column, 0..127.
 * @param[in] y Logical row, 0..63.
 * @param[in] progress Progress, 0..255, with exact empty/full sweep endpoints.
 * @return true if the moving upper-left to lower-right front has reached this pixel.
 */
static bool oled_startup_swept(size_t x, size_t y, uint8_t progress)
{
    static const uint8_t dither[4][4] = {
        {0U, 8U, 2U, 10U}, {12U, 4U, 14U, 6U}, {3U, 11U, 1U, 9U}, {15U, 7U, 13U, 5U}};
    const uint32_t reach = ((uint32_t)progress * (OLED_WIDTH + OLED_HEIGHT - 2U + 16U)) / 255U;
    const int32_t coverage = (int32_t)reach - (int32_t)(x + y);
    return (coverage > 0) && ((coverage >= 16) || ((uint32_t)coverage > dither[y % 4U][x % 4U]));
}
/**
 * @brief Release panel I/O before its owning I2C bus after failed initialization.
 * @note Called only by the UI task; no hardware reset line exists on the four-wire module.
 */
static void oled_release(void)
{
    oled_ready = false;
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

/**
 * @brief Set page and column explicitly, then transmit one bounded SH1106 RAM segment.
 * @param[in] page Eight-row page index, 0..7.
 * @param[in] column First RAM column, 0..131.
 * @param[in] data Borrowed page bytes, consumed by the synchronous I2C transfer.
 * @param[in] length Byte count; column + length must not exceed 132.
 * @return ESP_OK or the first I2C command/data transfer error.
 * @pre Panel I/O exists and the UI task owns all display calls.
 */
static esp_err_t oled_write_page(uint8_t page, uint8_t column, const uint8_t *data, size_t length)
{
    const uint8_t columns[] = {(uint8_t)(column & 0x0fU), (uint8_t)(0x10U | (column >> 4U))};
    esp_err_t result =
        esp_lcd_panel_io_tx_param(oled_io, (int)(0xb0U | page), columns, sizeof(columns));
    if (result == ESP_OK)
    {
        /* SH1106 has no SSD1306 column/page window or horizontal addressing commands. */
        result = esp_lcd_panel_io_tx_color(oled_io, -1, data, length);
    }
    return result;
}

/** @brief Flush all visible pages, stopping immediately on an I2C transfer failure. */
static esp_err_t oled_flush(void)
{
    esp_err_t result = ESP_OK;
    for (uint8_t page = 0U; (page < OLED_PAGES) && (result == ESP_OK); ++page)
    {
        result = oled_write_page(page, (uint8_t)CONFIG_PROBE_OLED_COLUMN_OFFSET,
                                 &frame[(size_t)page * OLED_WIDTH], OLED_WIDTH);
    }
    return result;
}

/**
 * @brief Configure SH1106, clear all 132 RAM columns and enable the 128x64 display.
 * @return ESP_OK after stabilization or the first I2C transfer error.
 * @pre Panel I/O exists; the UI task has exclusive ownership.
 * @note Commands follow the SH1106 V2.3 command table, using the module's internal DC-DC.
 */
static esp_err_t oled_initialize_controller(void)
{
    static const uint8_t configuration[] = {
        0xd5U, 0x80U, /* Display clock: divide by one, middle oscillator setting. */
        0xa8U, 0x3fU, /* Multiplex all 64 common outputs. */
        0xd3U, 0x00U, /* No vertical display offset. */
        0x40U,        /* Start line zero. */
        0xadU, 0x8bU, /* SH1106 internal DC-DC enabled; configure while display is off. */
        0xa0U, 0xc0U, /* Rotate 180 degrees relative to the standard A1/C8 module mapping. */
        0xdaU, 0x12U, /* Alternative common-pad layout. */
        0x81U, 0x80U, /* Initial contrast, 0..255. */
        0xd9U, 0x22U, /* Two display clocks for pre-charge and discharge. */
        0xdbU, 0x35U, /* SH1106 power-on VCOM deselect setting. */
        0xa4U, 0xa6U, /* Display RAM contents with normal pixel polarity. */
    };
    static const uint8_t empty_page[SH1106_RAM_COLUMNS] = {0};
    esp_err_t result =
        esp_lcd_panel_io_tx_param(oled_io, 0xae, configuration, sizeof(configuration));
    for (uint8_t page = 0U; (page < OLED_PAGES) && (result == ESP_OK); ++page)
    {
        result = oled_write_page(page, 0U, empty_page, sizeof(empty_page));
    }
    if (result == ESP_OK)
    {
        result = esp_lcd_panel_io_tx_param(oled_io, 0xaf, NULL, 0U);
    }
    if (result == ESP_OK)
    {
        /* The datasheet power sequence allows 100 ms for display output to settle. */
        vTaskDelay(pdMS_TO_TICKS(100U));
    }
    return result;
}
#endif
/**
 * @brief Probe supported I2C addresses and initialize the optional SH1106 controller.
 */
esp_err_t probe_oled_init(void)
{
    esp_err_t result = ESP_ERR_NOT_SUPPORTED;
#if CONFIG_PROBE_OLED_ENABLE
    if ((oled_io != NULL) || (oled_bus != NULL))
    {
        return ESP_ERR_INVALID_STATE;
    }
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
        result = oled_initialize_controller();
        oled_ready = (result == ESP_OK);
    }
    if (result != ESP_OK)
    {
        oled_release();
        result = ESP_ERR_NOT_FOUND;
    }
#endif
    return result;
}
/**
 * @brief Render supported glyphs into the fixed framebuffer and transfer the whole panel.
 */
esp_err_t probe_oled_draw(const char lines[][PROBE_OLED_TEXT_COLUMNS + 1U], size_t count)
{
    esp_err_t result = ESP_ERR_INVALID_STATE;
#if CONFIG_PROBE_OLED_ENABLE
    if (oled_ready && (lines != NULL) && (count <= OLED_PAGES))
    {
        (void)memset(frame, 0, sizeof(frame));
        for (size_t row = 0U; row < count; row++)
        {
            for (size_t column = 0U;
                 (column < PROBE_OLED_TEXT_COLUMNS) && (lines[row][column] != '\0'); column++)
            {
                const char character = lines[row][column];
                const uint8_t *const glyph = oled_glyph(character);
                if (glyph != NULL)
                {
                    (void)memcpy(&frame[(row * OLED_WIDTH) + (column * 8U)], glyph, 8U);
                }
            }
        }
        result = oled_flush();
    }
#else
    (void)lines;
    (void)count;
#endif
    return result;
}

/** @brief Compose and transmit one startup frame using the current logical image orientation. */
esp_err_t probe_oled_startup_frame(probe_oled_startup_phase_t phase, uint8_t progress)
{
    esp_err_t result = ESP_ERR_INVALID_STATE;
#if CONFIG_PROBE_OLED_ENABLE
    if (oled_ready && ((phase == PROBE_OLED_STARTUP_FILL) || (phase == PROBE_OLED_STARTUP_REVEAL)))
    {
        oled_build_startup_logo();
        (void)memset(frame, 0, sizeof(frame));
        for (size_t y = 0U; y < OLED_HEIGHT; ++y)
        {
            for (size_t x = 0U; x < OLED_WIDTH; ++x)
            {
                const bool swept = oled_startup_swept(x, y, progress);
                const bool logo =
                    (startup_logo[((y / 8U) * OLED_WIDTH) + x] & (uint8_t)(1U << (y % 8U))) != 0U;
                if (((phase == PROBE_OLED_STARTUP_FILL) && swept) ||
                    ((phase == PROBE_OLED_STARTUP_REVEAL) && (!swept || logo)))
                {
                    oled_pixel(frame, x, y);
                }
            }
        }
        const bool dark = (phase == PROBE_OLED_STARTUP_FILL) && (progress == 0U);
        const uint8_t brightness = (phase == PROBE_OLED_STARTUP_FILL)
                                       ? (uint8_t)(1U + (((uint32_t)progress * 127U) / 255U))
                                       : 128U;
        result = esp_lcd_panel_io_tx_param(oled_io, 0x81, &brightness, 1U);
        if (result == ESP_OK)
        {
            result = esp_lcd_panel_io_tx_param(oled_io, dark ? 0xae : 0xaf, NULL, 0U);
        }
        if (result == ESP_OK)
        {
            result = oled_flush();
        }
    }
#else
    (void)phase;
    (void)progress;
#endif
    return result;
}
/**
 * @brief Send a validated SH1106 contrast command from the display-owning task.
 */
esp_err_t probe_oled_contrast(unsigned int value)
{
    esp_err_t result = ESP_ERR_INVALID_ARG;
#if CONFIG_PROBE_OLED_ENABLE
    if (oled_ready && (value <= 255U))
    {
        const uint8_t contrast = (uint8_t)value;
        result = esp_lcd_panel_io_tx_param(oled_io, 0x81, &contrast, 1U);
    }
#else
    (void)value;
#endif
    return result;
}
