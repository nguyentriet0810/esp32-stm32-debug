/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_state.h
 * @brief Bounded task-safe snapshots shared by diagnostics and the local UI.
 */
#ifndef PROBE_STATE_H
#define PROBE_STATE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct
{
    bool station_connected;
    bool gdb_connected;
    bool target_running;
    bool oled_present;
    uint32_t swd_hz;
    uint32_t target_errors;
    char ssid[33];
    char ip[16];
    char ap_ssid[33];
    char target[32];
} probe_state_t;
/** @brief Copy a consistent snapshot. @param[out] state Non-NULL output. Task context. */
void probe_state_get(probe_state_t *state);
/**
 * @brief Publish network identity from task context; strings are copied, never retained.
 * @param[in] connected True after station DHCP completes.
 * @param[in] ssid Network name; NULL becomes empty, long input is truncated to 32 bytes.
 * @param[in] ip IPv4 text; NULL becomes empty, at most 15 bytes are copied.
 */
void probe_state_network(bool connected, const char *ssid, const char *ip);
/** @brief Copy the AP name, at most 32 bytes; NULL clears it. Task context only. */
void probe_state_ap(const char *ssid);
/**
 * @brief Publish debug session/target identity from task context.
 * @param[in] connected True while a client owns the debug session.
 * @param[in] running Backend target-running state.
 * @param[in] target Driver name, copied up to 31 bytes; NULL becomes empty.
 */
void probe_state_debug(bool connected, bool running, const char *target);
/** @brief Publish nominal port frequency in Hz from task context. */
void probe_state_clock(uint32_t hz);
/** @brief Publish OLED availability from task context. */
void probe_state_oled(bool present);
/** @brief Increment the saturating target error counter from task context. */
void probe_state_error(void);
#endif
