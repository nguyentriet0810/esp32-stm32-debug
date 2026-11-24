/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_state.h
 * @brief Synchronized diagnostic state snapshots API.
 *
 * A short task-context spinlock protects fixed-size state. Readers receive copies and never
 * retain pointers into mutable shared storage.
 */
#ifndef PROBE_STATE_H
#define PROBE_STATE_H
#include <stdbool.h>
#include <stdint.h>
/** @brief Value snapshot; every string is inline, bounded and NUL-terminated. */
typedef struct
{
    bool station_connected; /**< True after station DHCP completes. */
    bool gdb_connected;     /**< True while one GDB client owns the session. */
    bool target_running;    /**< Last run state published by the debug owner. */
    bool oled_present;      /**< Current display availability reported by the UI task. */
    uint32_t swd_hz;        /**< Selected nominal SWD clock in Hz. */
    uint32_t target_errors; /**< Saturating count of recovered target errors. */
    char ssid[33];          /**< Station SSID, at most 32 bytes plus terminator. */
    char ip[16];            /**< IPv4 text, at most 15 bytes plus terminator. */
    char ap_ssid[33];       /**< Fallback AP name, at most 32 bytes plus terminator. */
    char target[32];        /**< Backend driver name, at most 31 bytes plus terminator. */
} probe_state_t;
/**
 * @brief Copy one consistent diagnostic snapshot.
 * @param[out] state Caller-owned destination; NULL is ignored.
 * @note Task context only. Copies under a spinlock; no caller-owned storage is retained.
 */
void probe_state_get(probe_state_t *state);
/**
 * @brief Publish network identity from task context; strings are copied, never retained.
 * @param[in] connected True after station DHCP completes.
 * @param[in] ssid Network name; NULL becomes empty, long input is truncated to 32 bytes.
 * @param[in] ip IPv4 text; NULL becomes empty, at most 15 bytes are copied.
 */
void probe_state_network(bool connected, const char *ssid, const char *ip);
/**
 * @brief Publish a copied fallback AP name.
 * @param[in] ssid NUL-terminated name; NULL clears it, long input is truncated to 32 bytes.
 * @note Task context only; safe alongside other state readers and publishers.
 */
void probe_state_ap(const char *ssid);
/**
 * @brief Publish debug session/target identity from task context.
 * @param[in] connected True while a client owns the debug session.
 * @param[in] running Backend target-running state.
 * @param[in] target Driver name, copied up to 31 bytes; NULL becomes empty.
 */
void probe_state_debug(bool connected, bool running, const char *target);
/**
 * @brief Publish the nominal SWD port clock.
 * @param[in] hz Selected frequency in Hz; this API stores the value without clamping it.
 * @note Task context only; serialized with all other state operations.
 */
void probe_state_clock(uint32_t hz);
/**
 * @brief Publish whether the UI considers the display available.
 * @param[in] present True while the OLED is usable by its owning task.
 * @note Task context only; this changes diagnostic state without accessing I2C.
 */
void probe_state_oled(bool present);
/**
 * @brief Increment the target-error count with saturation at UINT32_MAX.
 * @note Task context only; serialized with other state operations.
 */
void probe_state_error(void);
#endif
