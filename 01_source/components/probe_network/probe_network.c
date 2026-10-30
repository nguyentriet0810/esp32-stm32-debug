/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_network.c @brief Always-available AP with optional persisted station mode. */
#include "probe_network.h"
#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "probe_state.h"
static const char network_tag[] = "probe_network";
static char ap_name[33];
static atomic_bool station_enabled;
/** @brief Network callbacks publish copied snapshots; no target access occurs here. */
static void network_event(void *context, esp_event_base_t base, int32_t id, void *data)
{
    (void)context;
    if ((base == WIFI_EVENT) && (id == WIFI_EVENT_STA_START) && atomic_load(&station_enabled))
    {
        (void)esp_wifi_connect();
    }
    else if ((base == WIFI_EVENT) && (id == WIFI_EVENT_STA_DISCONNECTED))
    {
        probe_state_t state = {0};
        probe_state_get(&state);
        probe_state_network(false, state.ssid, "192.168.4.1");
        if (atomic_load(&station_enabled))
        {
            (void)esp_wifi_connect();
        }
    }
    else if ((base == IP_EVENT) && (id == IP_EVENT_STA_GOT_IP) && (data != NULL))
    {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)data;
        char ip[16] = {0};
        (void)snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
        wifi_config_t station = {0};
        char name[33] = {0};
        if (esp_wifi_get_config(WIFI_IF_STA, &station) == ESP_OK)
        {
            (void)memcpy(name, station.sta.ssid, 32U);
        }
        probe_state_network(true, name, ip);
        ESP_LOGI(network_tag, "STA connected: ssid=%s ip=%s", name, ip);
    }
}
/** @brief Load station credentials without exposing them to diagnostics. */
static bool network_load_station(wifi_config_t *config)
{
    nvs_handle_t handle = 0U;
    bool found = false;
    if (nvs_open("probe_wifi", NVS_READONLY, &handle) == ESP_OK)
    {
        char name[33] = {0};
        size_t ssid_size = sizeof(name);
        size_t password_size = sizeof(config->sta.password);
        esp_err_t result = nvs_get_str(handle, "ssid", name, &ssid_size);
        if (result == ESP_OK)
        {
            (void)memcpy(config->sta.ssid, name, 32U);
        }
        if (result == ESP_OK)
        {
            result = nvs_get_str(handle, "password", (char *)config->sta.password, &password_size);
        }
        found = (result == ESP_OK);
        nvs_close(handle);
    }
    return found;
}
esp_err_t probe_network_start(void)
{
    esp_err_t result = esp_netif_init();
    if (result == ESP_OK)
    {
        result = esp_event_loop_create_default();
    }
    if (result == ESP_OK)
    {
        if ((esp_netif_create_default_wifi_ap() == NULL) ||
            (esp_netif_create_default_wifi_sta() == NULL))
        {
            result = ESP_ERR_NO_MEM;
        }
    }
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (result == ESP_OK)
    {
        result = esp_wifi_init(&init);
    }
    if (result == ESP_OK)
    {
        result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    }
    if (result == ESP_OK)
    {
        result = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, network_event, NULL);
    }
    if (result == ESP_OK)
    {
        result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, network_event, NULL);
    }
    uint8_t mac[6] = {0};
    if (result == ESP_OK)
    {
        result = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    }
    wifi_config_t ap = {0};
    wifi_config_t station = {0};
    char password[17] = {0};
    station.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    if (result == ESP_OK)
    {
        (void)snprintf(ap_name, sizeof(ap_name), "STM32-PROBE-%02X%02X", (unsigned int)mac[4],
                       (unsigned int)mac[5]);
        nvs_handle_t handle = 0U;
        result = nvs_open("probe_wifi", NVS_READWRITE, &handle);
        if (result == ESP_OK)
        {
            size_t length = sizeof(password);
            if (nvs_get_str(handle, "ap_password", password, &length) != ESP_OK)
            {
                (void)snprintf(password, sizeof(password), "P%08" PRIX32 "x", esp_random());
                result = nvs_set_str(handle, "ap_password", password);
                if (result == ESP_OK)
                {
                    result = nvs_commit(handle);
                }
            }
            nvs_close(handle);
        }
        const size_t name_length = strlen(ap_name);
        (void)memcpy(ap.ap.ssid, ap_name, name_length);
        ap.ap.ssid_len = (uint8_t)name_length;
        (void)snprintf((char *)ap.ap.password, sizeof(ap.ap.password), "%s", password);
        ap.ap.channel = 6U;
        ap.ap.max_connection = 2U;
        ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
        atomic_store(&station_enabled, network_load_station(&station));
    }
    if (result == ESP_OK)
    {
        result = esp_wifi_set_mode(WIFI_MODE_APSTA);
    }
    if (result == ESP_OK)
    {
        result = esp_wifi_set_config(WIFI_IF_AP, &ap);
    }
    if ((result == ESP_OK) && atomic_load(&station_enabled))
    {
        result = esp_wifi_set_config(WIFI_IF_STA, &station);
    }
    if (result == ESP_OK)
    {
        probe_state_ap(ap_name);
        char name[33] = {0};
        (void)memcpy(name, station.sta.ssid, 32U);
        probe_state_network(false, name, "192.168.4.1");
        result = esp_wifi_start();
    }
    if (result == ESP_OK)
    {
        (void)esp_wifi_set_ps(WIFI_PS_NONE);
        ESP_LOGI(network_tag, "AP ready: ssid=%s ip=192.168.4.1", ap_name);
        /* The AP password is provisioned in NVS; it is deliberately omitted from logs. */
    }
    return result;
}
esp_err_t probe_network_ap_credentials(char ssid[33], char password[64])
{
    esp_err_t result = ESP_ERR_INVALID_ARG;
    if ((ssid != NULL) && (password != NULL))
    {
        wifi_config_t config = {0};
        result = esp_wifi_get_config(WIFI_IF_AP, &config);
        if (result == ESP_OK)
        {
            (void)memcpy(ssid, config.ap.ssid, 32U);
            ssid[32] = '\0';
            (void)memcpy(password, config.ap.password, 63U);
            password[63] = '\0';
        }
    }
    return result;
}
esp_err_t probe_network_provision(const char *ssid, const char *password)
{
    esp_err_t result = ESP_ERR_INVALID_ARG;
    if ((ssid != NULL) && (password != NULL))
    {
        const size_t ssid_length = strlen(ssid);
        const size_t password_length = strlen(password);
        if ((ssid_length > 0U) && (ssid_length <= 32U) && (password_length >= 8U) &&
            (password_length <= 63U))
        {
            nvs_handle_t handle = 0U;
            result = nvs_open("probe_wifi", NVS_READWRITE, &handle);
            if (result == ESP_OK)
            {
                result = nvs_set_str(handle, "ssid", ssid);
                if (result == ESP_OK)
                {
                    result = nvs_set_str(handle, "password", password);
                }
                if (result == ESP_OK)
                {
                    result = nvs_commit(handle);
                }
                nvs_close(handle);
            }
            if (result == ESP_OK)
            {
                wifi_config_t config = {0};
                (void)memcpy(config.sta.ssid, ssid, ssid_length);
                (void)memcpy(config.sta.password, password, password_length);
                config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
                atomic_store(&station_enabled, false);
                (void)esp_wifi_disconnect();
                result = esp_wifi_set_config(WIFI_IF_STA, &config);
                if (result == ESP_OK)
                {
                    atomic_store(&station_enabled, true);
                    result = esp_wifi_connect();
                }
            }
        }
    }
    return result;
}
