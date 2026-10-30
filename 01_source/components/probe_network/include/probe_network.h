/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_network.h
 * @brief Wi-Fi AP/STA provisioning. Passwords are stored in NVS, not source files.
 */
#ifndef PROBE_NETWORK_H
#define PROBE_NETWORK_H
#include "esp_err.h"
/**
 * @brief Start the fallback AP and any stored station profile.
 * @pre Call once from the startup task after NVS initialization.
 * @return ESP_OK when Wi-Fi started; SDK initialization/storage errors otherwise.
 * @note Connection and DHCP completion are asynchronous. AP credential is generated once.
 */
esp_err_t probe_network_start(void);
/**
 * @brief Save and connect a station profile. Call only from the console task.
 * @param[in] ssid UTF-8 SSID, 1..32 bytes.
 * @param[in] password WPA password, 8..63 bytes. Never logged.
 * @return ESP_OK when configuration was applied; IP readiness is asynchronous.
 * @pre Caller holds the debug configuration reservation; not callable from ISR.
 */
esp_err_t probe_network_provision(const char *ssid, const char *password);
/**
 * @brief Read fallback AP credentials for an explicit local console request.
 * @param[out] ssid Caller-owned output, at least 33 bytes including terminator.
 * @param[out] password Caller-owned output, at least 64 bytes including terminator.
 * @return ESP_OK on success; SDK Wi-Fi error otherwise. Task context only.
 */
esp_err_t probe_network_ap_credentials(char ssid[33], char password[64]);
#endif
