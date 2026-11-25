/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_local_ui.c
 * @brief Local OLED and encoder menu.
 *
 * The UI task owns display operations and the provisional settings model.
 * Optional display failures are reported without stopping the debug service.
 */
#include "probe_local_ui.h"
#include <stdbool.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "probe_encoder.h"
#include "probe_oled.h"
#include "probe_state.h"
#include "probe_ui_settings.h"

_Static_assert(PROBE_UI_SETTINGS_ROWS == PROBE_OLED_TEXT_ROWS &&
                   PROBE_UI_SETTINGS_COLUMNS == PROBE_OLED_TEXT_COLUMNS,
               "Settings rows must match the OLED character grid");
static QueueHandle_t ui_events;

/**
 * @brief Discard startup-time input until a monotonic deadline without busy waiting.
 * @param[in] deadline_us Absolute ESP timer deadline in microseconds.
 * @note The input task remains responsive; queued actions are never replayed after startup.
 */
static void ui_startup_wait(int64_t deadline_us)
{
    int64_t remaining_us = deadline_us - esp_timer_get_time();
    while (remaining_us > 0)
    {
        const uint32_t wait_ms = (uint32_t)((remaining_us + INT64_C(999)) / INT64_C(1000));
        probe_encoder_event_t ignored;
        (void)xQueueReceive(ui_events, &ignored, pdMS_TO_TICKS(wait_ms));
        remaining_us = deadline_us - esp_timer_get_time();
    }
}

/**
 * @brief Run the bounded startup sequence before the settings screen.
 * @return ESP_OK when complete or the first OLED error; optional failures leave debug usable.
 * @note Only the core-0 UI task waits here. Network, console and the SWD owner keep running.
 */
static esp_err_t ui_startup(void)
{
    esp_err_t result = probe_oled_startup_frame(PROBE_OLED_STARTUP_FILL, 0U);
    if (result == ESP_OK)
    {
        ui_startup_wait(esp_timer_get_time() + INT64_C(250000));
    }
    for (unsigned int stage = 0U; (stage < 2U) && (result == ESP_OK); ++stage)
    {
        const probe_oled_startup_phase_t phase =
            (stage == 0U) ? PROBE_OLED_STARTUP_FILL : PROBE_OLED_STARTUP_REVEAL;
        for (uint32_t step = 1U; (step <= 16U) && (result == ESP_OK); ++step)
        {
            const int64_t next_frame_us = esp_timer_get_time() + INT64_C(120000);
            result = probe_oled_startup_frame(phase, (uint8_t)((step * 255U) / 16U));
            if (result == ESP_OK)
            {
                ui_startup_wait(next_frame_us);
            }
        }
        if ((stage == 0U) && (result == ESP_OK))
        {
            ui_startup_wait(esp_timer_get_time() + INT64_C(150000));
        }
    }
    if (result == ESP_OK)
    {
        ui_startup_wait(esp_timer_get_time() + INT64_C(1200000));
    }
    (void)xQueueReset(ui_events);
    return result;
}

/**
 * @brief Translate one hardware event into a platform-independent settings action.
 * @param[in,out] settings Initialized model owned by the UI task.
 * @param[in] event Debounced event copied from the encoder queue.
 * @return True when the menu needs to be redrawn; unknown events are ignored.
 */
static bool ui_settings_event(probe_ui_settings_t *settings, probe_encoder_event_t event)
{
    bool changed = false;
    switch (event)
    {
    case PROBE_ENCODER_LEFT:
        changed = probe_ui_settings_input(settings, PROBE_UI_SETTINGS_PREVIOUS);
        break;
    case PROBE_ENCODER_RIGHT:
        changed = probe_ui_settings_input(settings, PROBE_UI_SETTINGS_NEXT);
        break;
    case PROBE_ENCODER_CLICK:
        changed = probe_ui_settings_input(settings, PROBE_UI_SETTINGS_SELECT);
        break;
    case PROBE_ENCODER_BACK:
        changed = probe_ui_settings_input(settings, PROBE_UI_SETTINGS_CANCEL);
        break;
    default:
        break;
    }
    return changed;
}

/**
 * @brief Own OLED rendering and consume queued menu events on core 0.
 * @param[in] context Unused FreeRTOS task argument.
 * @note Runs startup first, then draws changed settings at most once per 200 ms. Mode choices
 *       stay in RAM; OLED contrast previews apply immediately and cancel restores the old value.
 *       An OLED error disables rendering for the remaining task lifetime.
 */
static void ui_task(void *context)
{
    (void)context;
    probe_ui_settings_t settings;
    probe_ui_settings_init(&settings);
    bool dirty = true;
    unsigned int applied_contrast = 128U;
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
    if (oled_ok)
    {
        const esp_err_t startup = ui_startup();
        if (startup != ESP_OK)
        {
            oled_ok = false;
            probe_state_oled(false);
            ESP_LOGW("probe_ui", "Startup display failed: %s", esp_err_to_name(startup));
        }
        else
        {
            ESP_LOGI("probe_ui", "Startup complete: MAGIC DEBUGGER TOOL");
        }
    }
    for (;;)
    {
        probe_encoder_event_t event = PROBE_ENCODER_BACK;
        if (xQueueReceive(ui_events, &event, pdMS_TO_TICKS(50U)) == pdTRUE)
        {
            dirty = ui_settings_event(&settings, event) || dirty;
        }
        const int64_t now_us = esp_timer_get_time();
        if (oled_ok && dirty && ((now_us - last_render_us) >= INT64_C(200000)))
        {
            last_render_us = now_us;
            char lines[PROBE_UI_SETTINGS_ROWS][PROBE_UI_SETTINGS_COLUMNS + 1U];
            probe_ui_settings_render(&settings, lines);
            const unsigned int contrast =
                ((unsigned int)probe_ui_settings_oled_percent(&settings) * 255U + 50U) / 100U;
            esp_err_t display_result = ESP_OK;
            if (contrast != applied_contrast)
            {
                display_result = probe_oled_contrast(contrast);
                if (display_result == ESP_OK)
                {
                    applied_contrast = contrast;
                }
            }
            if (display_result == ESP_OK)
            {
                display_result = probe_oled_draw(lines, PROBE_UI_SETTINGS_ROWS);
            }
            if (display_result != ESP_OK)
            {
                oled_ok = false;
                probe_state_oled(false);
                ESP_LOGW("probe_ui", "OLED went offline; debug remains available");
            }
            dirty = false;
        }
    }
}
/**
 * @brief Create the UI event queue, optional input task and display-owning UI task.
 */
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
