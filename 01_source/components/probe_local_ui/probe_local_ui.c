/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_local_ui.c @brief Read-only session status and idle-only clock settings. */
#include "probe_local_ui.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "probe_debug.h"
#include "probe_encoder.h"
#include "probe_oled.h"
#include "probe_state.h"
static QueueHandle_t ui_events;
static void ui_task(void *context)
{
    (void)context;
    const uint32_t clocks[] = {100000U, 400000U, 1000000U, 2000000U, 4000000U, 8000000U};
    size_t clock_index = 1U;
    bool clock_page = false;
    int64_t last_render_us = 0;
    bool oled_ok = (probe_oled_init() == ESP_OK);
    probe_state_oled(oled_ok);
#if CONFIG_PROBE_ENCODER_ENABLE
    const char *input_status = "encoder input task active";
#else
    const char *input_status = "encoder disabled";
#endif
    ESP_LOGI("probe_ui", "OLED %s; %s", oled_ok ? "online" : "not attached (nonfatal)",
             input_status);
    for (;;)
    {
        probe_encoder_event_t event = PROBE_ENCODER_BACK;
        if (xQueueReceive(ui_events, &event, pdMS_TO_TICKS(200U)) == pdTRUE)
        {
            if (event == PROBE_ENCODER_BACK)
            {
                clock_page = false;
            }
            else if (event == PROBE_ENCODER_CLICK)
            {
                if (clock_page)
                {
                    const esp_err_t result = probe_debug_set_clock(clocks[clock_index]);
                    ESP_LOGI("probe_ui", "Clock request: %s", esp_err_to_name(result));
                }
                else
                {
                    clock_page = true;
                }
            }
            else if (clock_page)
            {
                if (event == PROBE_ENCODER_RIGHT)
                {
                    clock_index = (clock_index + 1U) % (sizeof(clocks) / sizeof(clocks[0]));
                }
                else
                {
                    clock_index = (clock_index == 0U) ? ((sizeof(clocks) / sizeof(clocks[0])) - 1U)
                                                      : clock_index - 1U;
                }
            }
        }
        probe_state_t state = {0};
        probe_state_get(&state);
        const int64_t now_us = esp_timer_get_time();
        if (oled_ok && ((now_us - last_render_us) >= INT64_C(200000)))
        {
            last_render_us = now_us;
            char lines[8][22] = {{0}};
            (void)snprintf(lines[0], sizeof(lines[0]), "STM32 PROBE");
            (void)snprintf(lines[1], sizeof(lines[1]), "WIFI: %s",
                           state.station_connected ? "CONNECTED" : "SETUP AP");
            (void)snprintf(lines[2], sizeof(lines[2]), "IP: %.16s", state.ip);
            (void)snprintf(lines[3], sizeof(lines[3]), "MCU: %.16s",
                           (state.target[0] != '\0') ? state.target : "NONE");
            (void)snprintf(lines[4], sizeof(lines[4]), "SWD: %" PRIu32 " KHZ",
                           state.swd_hz / 1000U);
            (void)snprintf(lines[5], sizeof(lines[5]), "GDB: %s",
                           state.gdb_connected ? "BUSY" : "IDLE");
            if (clock_page)
            {
                (void)snprintf(lines[6], sizeof(lines[6]), "> CLOCK %" PRIu32 " KHZ",
                               clocks[clock_index] / 1000U);
                (void)snprintf(lines[7], sizeof(lines[7]), "PRESS APPLY/HOLD BACK");
            }
            else
            {
                (void)snprintf(lines[7], sizeof(lines[7]), "PRESS FOR CLOCK MENU");
            }
            if (probe_oled_draw(lines, 8U) != ESP_OK)
            {
                oled_ok = false;
                probe_state_oled(false);
                ESP_LOGW("probe_ui", "OLED went offline; debug remains available");
            }
        }
    }
}
esp_err_t probe_local_ui_start(void)
{
    ui_events = xQueueCreate(32U, sizeof(probe_encoder_event_t));
    esp_err_t result = (ui_events == NULL) ? ESP_ERR_NO_MEM : ESP_OK;
    if (result == ESP_OK)
    {
        const esp_err_t encoder = probe_encoder_start(ui_events);
        if ((encoder != ESP_OK) && (encoder != ESP_ERR_NOT_SUPPORTED))
        {
            result = encoder;
        }
    }
    if (result == ESP_OK)
    {
        result = (xTaskCreatePinnedToCore(ui_task, "local_ui", 4096U, NULL, 1U, NULL, 0) == pdPASS)
                     ? ESP_OK
                     : ESP_ERR_NO_MEM;
    }
    return result;
}
