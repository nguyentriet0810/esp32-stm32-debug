/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_state.c @brief Fixed-size state protected by an ESP-IDF spinlock. */
#include "probe_state.h"
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static probe_state_t current_state;
/** @brief Bounded copy; called while the task-only state lock is held. */
static void state_copy(char *out, size_t capacity, const char *text)
{
    size_t index = 0U;
    if ((out != NULL) && (capacity > 0U))
    {
        if (text != NULL)
        {
            while ((index < (capacity - 1U)) && (text[index] != '\0'))
            {
                out[index] = text[index];
                index++;
            }
        }
        out[index] = '\0';
    }
}
void probe_state_get(probe_state_t *state)
{
    if (state != NULL)
    {
        portENTER_CRITICAL(&state_lock);
        *state = current_state;
        portEXIT_CRITICAL(&state_lock);
    }
}
void probe_state_network(bool connected, const char *ssid, const char *ip)
{
    portENTER_CRITICAL(&state_lock);
    current_state.station_connected = connected;
    state_copy(current_state.ssid, sizeof(current_state.ssid), ssid);
    state_copy(current_state.ip, sizeof(current_state.ip), ip);
    portEXIT_CRITICAL(&state_lock);
}
void probe_state_ap(const char *ssid)
{
    portENTER_CRITICAL(&state_lock);
    state_copy(current_state.ap_ssid, sizeof(current_state.ap_ssid), ssid);
    portEXIT_CRITICAL(&state_lock);
}
void probe_state_debug(bool connected, bool running, const char *target)
{
    portENTER_CRITICAL(&state_lock);
    current_state.gdb_connected = connected;
    current_state.target_running = running;
    state_copy(current_state.target, sizeof(current_state.target), target);
    portEXIT_CRITICAL(&state_lock);
}
void probe_state_clock(uint32_t hz)
{
    portENTER_CRITICAL(&state_lock);
    current_state.swd_hz = hz;
    portEXIT_CRITICAL(&state_lock);
}
void probe_state_oled(bool present)
{
    portENTER_CRITICAL(&state_lock);
    current_state.oled_present = present;
    portEXIT_CRITICAL(&state_lock);
}
void probe_state_error(void)
{
    portENTER_CRITICAL(&state_lock);
    if (current_state.target_errors < UINT32_MAX)
    {
        current_state.target_errors++;
    }
    portEXIT_CRITICAL(&state_lock);
}
