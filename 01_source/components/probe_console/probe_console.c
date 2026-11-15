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
#include "probe_sd.h"
static void console_sd_status(void)
{
    probe_sd_status_t sd = {0};
    const esp_err_t result = probe_sd_get_status(&sd);
    if (result == ESP_OK)
    {
        (void)printf("SD_STATUS enabled=%s present=%s mounted=%s bytes=%" PRIu64 " last=%s\n",
                     sd.enabled ? "YES" : "NO", sd.present ? "YES" : "NO",
                     sd.mounted ? "YES" : "NO", sd.capacity_bytes, esp_err_to_name(sd.last_error));
    }
    else
    {
        (void)printf("SD_STATUS %s\n", esp_err_to_name(result));
    }
}
static void console_sd_command(char *argument)
{
    if ((argument[0] == '\0') || (strcmp(argument, "status") == 0))
    {
        console_sd_status();
    }
    else if (strcmp(argument, "mount") == 0)
    {
        (void)printf("SD_MOUNT %s\n", esp_err_to_name(probe_sd_mount()));
        console_sd_status();
    }
    else if (strcmp(argument, "unmount") == 0)
    {
        (void)printf("SD_UNMOUNT %s\n", esp_err_to_name(probe_sd_unmount()));
    }
    else if (strcmp(argument, "ls") == 0)
    {
        probe_sd_entry_t entries[PROBE_SD_MAX_LIST_ENTRIES] = {0};
        size_t count = 0U;
        bool truncated = false;
        const esp_err_t result =
            probe_sd_list(entries, PROBE_SD_MAX_LIST_ENTRIES, &count, &truncated);
        (void)printf("SD_LIST %s count=%zu truncated=%s\n", esp_err_to_name(result), count,
                     truncated ? "YES" : "NO");
        if (result == ESP_OK)
        {
            for (size_t index = 0U; index < count; ++index)
            {
                (void)printf("SD_ENTRY %s %s %" PRIu64 "\n",
                             entries[index].directory ? "DIR" : "FILE", entries[index].name,
                             entries[index].size_bytes);
            }
        }
    }
    else if (strncmp(argument, "read ", 5U) == 0)
    {
        uint8_t data[PROBE_SD_MAX_IO_BYTES] = {0};
        size_t length = 0U;
        bool truncated = false;
        const esp_err_t result =
            probe_sd_read(&argument[5], data, sizeof(data), &length, &truncated);
        (void)printf("SD_READ %s bytes=%zu truncated=%s\n", esp_err_to_name(result), length,
                     truncated ? "YES" : "NO");
        if (result == ESP_OK)
        {
            (void)printf("SD_DATA ");
            for (size_t index = 0U; index < length; ++index)
            {
                if ((data[index] >= 0x20U) && (data[index] <= 0x7eU) && (data[index] != '\\'))
                {
                    (void)putchar((int)data[index]);
                }
                else
                {
                    (void)printf("\\x%02x", (unsigned int)data[index]);
                }
            }
            (void)putchar('\n');
        }
    }
    else if (strncmp(argument, "write ", 6U) == 0)
    {
        char *separator = strchr(&argument[6], '|');
        esp_err_t result = ESP_ERR_INVALID_ARG;
        if (separator != NULL)
        {
            *separator = '\0';
            result = probe_sd_write(&argument[6], separator + 1, strlen(separator + 1));
        }
        (void)printf("SD_WRITE %s\n", esp_err_to_name(result));
    }
    else
    {
        (void)printf("SD_COMMANDS: sd [status|mount|unmount|ls] | sd read <8.3-name> | "
                     "sd write <8.3-name>|<text>\n");
    }
}
static void console_status(void)
{
    probe_state_t state = {0};
    probe_state_get(&state);
    probe_sd_status_t sd = {0};
    const esp_err_t sd_result = probe_sd_get_status(&sd);
    const char *sd_status = "ERROR";
    if (sd_result == ESP_ERR_NOT_SUPPORTED)
    {
        sd_status = "DISABLED";
    }
    else if (sd_result == ESP_OK)
    {
        sd_status = sd.mounted ? "MOUNTED" : (sd.present ? "UNMOUNTED" : "ABSENT");
    }
    (void)printf("STATUS wifi=%s ssid=%s ip=%s gdb=%s target=%s swd=%" PRIu32
                 " oled=%s errors=%" PRIu32 " heap=%zu sd=%s\n",
                 state.station_connected ? "STA" : "AP", state.ssid, state.ip,
                 state.gdb_connected ? "BUSY" : "IDLE",
                 (state.target[0] != '\0') ? state.target : "NONE", state.swd_hz,
                 state.oled_present ? "ONLINE" : "OFFLINE", state.target_errors,
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT), sd_status);
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
        (void)printf("PINS SD_CS=%d SD_MOSI=%d SD_SCK=%d SD_MISO=%d SD_CD=%d\n",
                     CONFIG_PROBE_SD_CS_GPIO, CONFIG_PROBE_SD_MOSI_GPIO, CONFIG_PROBE_SD_SCK_GPIO,
                     CONFIG_PROBE_SD_MISO_GPIO, CONFIG_PROBE_SD_DETECT_GPIO);
    }
    else if (strcmp(line, "sd") == 0)
    {
        console_sd_command(&line[2]);
    }
    else if (strncmp(line, "sd ", 3U) == 0)
    {
        console_sd_command(&line[3]);
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
        (void)printf("COMMANDS: status | pins | ap | clock <Hz> | wifi <ssid>|<password> | "
                     "sd [status|mount|unmount|ls] | sd read <8.3-name> | "
                     "sd write <8.3-name>|<text>\n");
    }
    (void)fflush(stdout);
}
static void console_task(void *context)
{
    (void)context;
    char line[384] = {0};
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
    return (xTaskCreatePinnedToCore(console_task, "console", 6144U, NULL, 2U, NULL, 0) == pdPASS)
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}
