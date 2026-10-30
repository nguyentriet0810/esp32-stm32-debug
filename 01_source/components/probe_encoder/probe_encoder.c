/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_encoder.c @brief Debounced input task; never performs I2C or SWD. */
#include "probe_encoder.h"
#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "probe_encoder_decode.h"
#if CONFIG_PROBE_ENCODER_ENABLE
static QueueHandle_t event_queue;
static void encoder_emit(probe_encoder_event_t event)
{
    if (xQueueSend(event_queue, &event, 0U) != pdTRUE)
    {
        ESP_LOGW("probe_encoder", "UI input queue overflow");
    }
}
static uint8_t encoder_sample(void)
{
    return (uint8_t)(((uint32_t)gpio_get_level(CONFIG_PROBE_ENCODER_A_GPIO) << 1U) |
                     (uint32_t)gpio_get_level(CONFIG_PROBE_ENCODER_B_GPIO));
}
static void encoder_task(void *context)
{
    (void)context;
    probe_encoder_decoder_t decoder = {.previous = encoder_sample(), .accumulator = 0};
    int raw_button = 1;
    int stable_button = 1;
    int64_t changed_us = esp_timer_get_time();
    int64_t pressed_us = 0;
    bool hold_sent = false;
    TickType_t wake = xTaskGetTickCount();
    for (;;)
    {
        const int8_t movement = probe_encoder_decode(&decoder, encoder_sample());
        if (movement > 0)
        {
            encoder_emit(PROBE_ENCODER_RIGHT);
        }
        else if (movement < 0)
        {
            encoder_emit(PROBE_ENCODER_LEFT);
        }
        const int button = gpio_get_level(CONFIG_PROBE_ENCODER_SW_GPIO);
        const int64_t now = esp_timer_get_time();
        if (button != raw_button)
        {
            raw_button = button;
            changed_us = now;
        }
        if ((raw_button != stable_button) && ((now - changed_us) >= INT64_C(20000)))
        {
            stable_button = raw_button;
            if (stable_button == 0)
            {
                pressed_us = now;
                hold_sent = false;
            }
            else if (!hold_sent)
            {
                encoder_emit(PROBE_ENCODER_CLICK);
            }
        }
        if ((stable_button == 0) && !hold_sent && ((now - pressed_us) >= INT64_C(750000)))
        {
            hold_sent = true;
            encoder_emit(PROBE_ENCODER_BACK);
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1U));
    }
}
#endif
esp_err_t probe_encoder_start(QueueHandle_t events)
{
    esp_err_t result = ESP_ERR_INVALID_ARG;
#if CONFIG_PROBE_ENCODER_ENABLE
    if (events != NULL)
    {
        const gpio_config_t pins = {.pin_bit_mask = (UINT64_C(1) << CONFIG_PROBE_ENCODER_A_GPIO) |
                                                    (UINT64_C(1) << CONFIG_PROBE_ENCODER_B_GPIO) |
                                                    (UINT64_C(1) << CONFIG_PROBE_ENCODER_SW_GPIO),
                                    .mode = GPIO_MODE_INPUT,
                                    .pull_up_en = GPIO_PULLUP_ENABLE,
                                    .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                    .intr_type = GPIO_INTR_DISABLE};
        result = gpio_config(&pins);
        if (result == ESP_OK)
        {
            event_queue = events;
            result = (xTaskCreatePinnedToCore(encoder_task, "encoder", 3072U, NULL, 4U, NULL, 0) ==
                      pdPASS)
                         ? ESP_OK
                         : ESP_ERR_NO_MEM;
        }
    }
#else
    (void)events;
    result = ESP_ERR_NOT_SUPPORTED;
#endif
    return result;
}
