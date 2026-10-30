/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_console.c @brief Serial provisioning and observable service diagnostics. */
#include "probe_console.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "probe_debug.h"
#include "probe_network.h"
#include "probe_state.h"
static void console_status(void)
{
    probe_state_t state = {0};
    probe_state_get(&state);
    (void)printf("STATUS wifi=%s ssid=%s ip=%s gdb=%s target=%s swd=%" PRIu32
                 " oled=%s errors=%" PRIu32 " heap=%zu\n",
                 state.station_connected ? "STA" : "AP", state.ssid, state.ip,
                 state.gdb_connected ? "BUSY" : "IDLE",
                 (state.target[0] != '\0') ? state.target : "NONE", state.swd_hz,
                 state.oled_present ? "ONLINE" : "OFFLINE", state.target_errors,
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}
static void console_command(char *line)
{
    if (strcmp(line, "status") == 0)
    {
        console_status();
    }
    else if (strcmp(line, "ap") == 0)
    {
        char ssid[33] = {0};
        char password[64] = {0};
        const esp_err_t result = probe_network_ap_credentials(ssid, password);
        if (result == ESP_OK)
        {
            /* Explicit local request only; never included in automatic diagnostics. */
            (void)printf("AP ssid=%s password=%s ip=192.168.4.1\n", ssid, password);
        }
        else
        {
            (void)printf("AP_CONFIG %s\n", esp_err_to_name(result));
        }
        (void)memset(password, 0, sizeof(password));
    }
    else if (strcmp(line, "pins") == 0)
    {
        (void)printf("PINS SWDIO=%d SWCLK=%d SWO=%d NRST=%d OLED=%d,%d ENC=%d,%d,%d\n",
                     CONFIG_PROBE_SWDIO_GPIO, CONFIG_PROBE_SWCLK_GPIO, CONFIG_PROBE_SWO_GPIO,
                     CONFIG_PROBE_NRST_GPIO, CONFIG_PROBE_OLED_SDA_GPIO, CONFIG_PROBE_OLED_SCL_GPIO,
                     CONFIG_PROBE_ENCODER_A_GPIO, CONFIG_PROBE_ENCODER_B_GPIO,
                     CONFIG_PROBE_ENCODER_SW_GPIO);
    }
    else if (strncmp(line, "wifi ", 5U) == 0)
    {
        char *separator = strchr(&line[5], '|');
        esp_err_t result = ESP_ERR_INVALID_ARG;
        if (separator != NULL)
        {
            if (probe_debug_configuration_lock())
            {
                *separator = '\0';
                result = probe_network_provision(&line[5], separator + 1);
                probe_debug_configuration_unlock();
            }
            else
            {
                result = ESP_ERR_INVALID_STATE;
            }
        }
        (void)printf("WIFI_CONFIG %s\n", esp_err_to_name(result));
    }
    else if (strncmp(line, "clock ", 6U) == 0)
    {
        char *end = NULL;
        errno = 0;
        const unsigned long hz = strtoul(&line[6], &end, 10);
        esp_err_t result = ESP_ERR_INVALID_ARG;
        if ((errno == 0) && (end != &line[6]) && (*end == '\0') && (hz <= 8000000UL))
        {
            result = probe_debug_set_clock((uint32_t)hz);
        }
        (void)printf("CLOCK_CONFIG %s\n", esp_err_to_name(result));
    }
    else
    {
        (void)printf("COMMANDS: status | pins | ap | clock <Hz> | wifi <ssid>|<password>\n");
    }
    (void)fflush(stdout);
}
static void console_task(void *context)
{
    (void)context;
    char line[160] = {0};
    size_t length = 0U;
    bool overflow = false;
    (void)printf("CONSOLE_READY\n");
    for (;;)
    {
        const int character = fgetc(stdin);
        if (character == EOF)
        {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(10U));
        }
        else if ((character == '\n') || (character == '\r'))
        {
            if ((length > 0U) && !overflow)
            {
                line[length] = '\0';
                console_command(line);
            }
            (void)memset(line, 0, sizeof(line));
            length = 0U;
            overflow = false;
        }
        else if ((length < (sizeof(line) - 1U)) && !overflow)
        {
            line[length++] = (char)character;
        }
        else
        {
            overflow = true;
        }
    }
}
esp_err_t probe_console_start(void)
{
    return (xTaskCreatePinnedToCore(console_task, "console", 4096U, NULL, 2U, NULL, 0) == pdPASS)
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}
