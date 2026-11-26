/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_network.h
 * @brief Fallback Wi-Fi AP and station provisioning API.
 *
 * Credentials are stored in NVS; event callbacks publish copied state snapshots. Connection and
 * DHCP completion happen asynchronously.
 */
#ifndef PROBE_NETWORK_H
#define PROBE_NETWORK_H
#include "esp_err.h"
/**
 * @brief Start the fallback AP and any stored station profile.
 * @pre Call once from the startup task after NVS initialization.
 * @return ESP_OK when Wi-Fi started; SDK initialization/storage errors otherwise.
 * @note Connection/DHCP are asynchronous. The setup AP uses MAGIC-PROBE / 1234567890.
 */
esp_err_t probe_network_start(void);
/**
 * @brief Persist a station profile and request a connection from a provisioning task.
 * @param[in] ssid Borrowed NUL-terminated UTF-8 SSID, 1..32 bytes.
 * @param[in] password Borrowed NUL-terminated WPA password, 8..63 bytes; never logged.
 * @retval ESP_OK The profile was saved and the connection request was accepted.
 * @retval ESP_ERR_INVALID_ARG A pointer or credential length is invalid.
 * @return Other NVS or Wi-Fi errors are returned unchanged.
 * @pre Network initialization completed and the caller holds the debug reservation.
 * @note Task context only. IP readiness is asynchronous. A Wi-Fi error after NVS commit
 *       does not roll back the saved credentials; input pointers are not retained.
 */
esp_err_t probe_network_provision(const char *ssid, const char *password);
/**
 * @brief Copy AP credentials for an explicit console request or the opened Wi-Fi UI page.
 * @param[out] ssid Caller-owned output with at least 33 bytes including its terminator.
 * @param[out] password Caller-owned output with at least 64 bytes including its terminator.
 * @retval ESP_OK Both terminated outputs were copied.
 * @retval ESP_ERR_INVALID_ARG Either output pointer is NULL.
 * @return Wi-Fi query errors are returned unchanged; outputs are written only on success.
 * @pre Wi-Fi initialization completed. Call from task context.
 * @note The caller controls display and lifetime of the copied password.
 */
esp_err_t probe_network_ap_credentials(char ssid[33], char password[64]);
#endif
