/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file probe_wifi_setup.h @brief Phone provisioning portal and asynchronous router ping. */
#ifndef PROBE_WIFI_SETUP_H
#define PROBE_WIFI_SETUP_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define PROBE_WIFI_SCAN_MAX 16U

/** @brief Progress of the most recent requested 2.4 GHz scan. */
typedef enum
{
    PROBE_WIFI_SCAN_IDLE,
    PROBE_WIFI_SCAN_RUNNING,
    PROBE_WIFI_SCAN_DONE,
    PROBE_WIFI_SCAN_ERROR
} probe_wifi_scan_state_t;

/** @brief One visible SSID, strongest RSSI in dBm and whether authentication is required. */
typedef struct
{
    char ssid[33];
    int8_t rssi;
    bool secured;
} probe_wifi_scan_entry_t;

/** @brief Bounded copied scan results; entries are ordered by descending signal strength. */
typedef struct
{
    probe_wifi_scan_state_t state;
    esp_err_t result;
    uint32_t revision;
    uint16_t count;
    probe_wifi_scan_entry_t entries[PROBE_WIFI_SCAN_MAX];
} probe_wifi_scan_results_t;

/** @brief Result of the latest three-packet gateway ping. */
typedef enum
{
    PROBE_WIFI_PING_IDLE,
    PROBE_WIFI_PING_RUNNING,
    PROBE_WIFI_PING_OK,
    PROBE_WIFI_PING_NO_REPLY,
    PROBE_WIFI_PING_NO_WIFI,
    PROBE_WIFI_PING_ERROR
} probe_wifi_ping_state_t;
/** @brief Copied service state; never includes Wi-Fi passwords. */
typedef struct
{
    bool ready;
    bool enabled; /**< Web configuration mode; independent of station connectivity. */
    bool busy;
    bool configuring;
    esp_err_t configure_result;
    probe_wifi_ping_state_t ping;
    uint32_t sent;
    uint32_t received;
    uint32_t mean_ms;
    char gateway[16];
    probe_wifi_scan_state_t scan;
    uint32_t scan_revision;
    uint16_t scan_count;
    esp_err_t scan_result;
} probe_wifi_setup_status_t;
/**
 * @brief Start the AP-bound HTTP portal and bounded provisioning worker once.
 * @return ESP_OK on success, otherwise an SDK allocation/bind/registration error.
 * @pre Network and debug services are initialized. Task context only.
 */
esp_err_t probe_wifi_setup_start(void);
/**
 * @brief Enable or disable all AP web configuration routes without reconnecting STA.
 * @param[in] enabled True opens setup; false rejects web configuration requests.
 * @return ESP_OK on success; ESP_ERR_INVALID_STATE when unavailable/busy or OFF without STA.
 * @note Task context only; no Wi-Fi mode, router profile or NVS change. Missing STA forces ON
 *       and keeps ON after connection until explicitly disabled while STA is connected.
 */
esp_err_t probe_wifi_setup_set_enabled(bool enabled);
/**
 * @brief Start three ICMP echo requests to the DHCP gateway on the station interface.
 * @return ESP_OK if started; ESP_ERR_INVALID_STATE when busy/offline; other SDK errors.
 * @note Task context only, nonblocking. Results are copied through the status API.
 */
esp_err_t probe_wifi_setup_ping_router(void);
/**
 * @brief Queue a visible-network scan in the setup worker, without blocking HTTP or UI.
 * @return ESP_OK if queued; ESP_ERR_INVALID_STATE if unavailable/busy; other queue errors.
 * @note Task context only. Reserves idle debug ownership until scanning completes.
 */
esp_err_t probe_wifi_setup_scan(void);
/**
 * @brief Copy the latest scan state and at most 16 distinct visible SSIDs.
 * @param[out] results Non-NULL caller-owned snapshot, copied under a short lock.
 * @note Task context only; no retained pointer, password, network connection or NVS write.
 */
void probe_wifi_setup_get_scan(probe_wifi_scan_results_t *results);
/**
 * @brief Copy the current portal/configuration/ping state without retaining a pointer.
 * @param[out] status Non-NULL caller-owned snapshot.
 * @note Task context only; synchronized with HTTP, worker and ping callbacks.
 */
void probe_wifi_setup_get_status(probe_wifi_setup_status_t *status);
#endif
