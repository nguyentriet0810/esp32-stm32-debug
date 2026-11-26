/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file probe_wifi_setup.c
 * @brief AP-only phone setup server and three-packet station-gateway diagnostics.
 *
 * HTTP never returns passwords. A worker applies copied credentials while holding the same
 * idle reservation as console provisioning. Ping callbacks own session cleanup and publish
 * bounded snapshots. UI callers never wait for DHCP or ping completion.
 */
#include "probe_wifi_setup.h"
#include "probe_wifi_form.h"
#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "ping/ping_sock.h"
#include "probe_debug.h"
#include "probe_network.h"
#include "probe_state.h"

/* SDK-006: ESP-IDF's embedded text asset is supplied by the component linker. */
extern const char portal_html[] __asm__("_binary_wifi_portal_html_start");
static httpd_handle_t setup_server;
/** @brief The single worker serializes provisioning and scans using copied job data. */
typedef struct
{
    bool scan;
    probe_wifi_credentials_t credentials;
} setup_job_t;
static QueueHandle_t setup_queue;
static TaskHandle_t setup_worker;
static atomic_bool setup_busy;
static atomic_bool setup_enabled;
static portMUX_TYPE setup_lock = portMUX_INITIALIZER_UNLOCKED;
static probe_wifi_setup_status_t setup_status;
static uint32_t ping_total_ms;
static char csrf_token[33];
static probe_wifi_scan_results_t scan_results;

/** @brief A setup mode can only be switched OFF when a named station has obtained an IP. */
static bool setup_has_station(void)
{
    probe_state_t state = {0};
    probe_state_get(&state);
    return state.station_connected && (state.ssid[0] != '\0');
}

/** @brief Keep the setup portal reachable whenever the station has no active connection. */
static bool setup_web_enabled(void)
{
    return atomic_load(&setup_enabled) || !setup_has_station();
}

/** @brief Copy status under a short lock; no driver operation runs in the critical section. */
void probe_wifi_setup_get_status(probe_wifi_setup_status_t *status)
{
    portENTER_CRITICAL(&setup_lock);
    *status = setup_status;
    portEXIT_CRITICAL(&setup_lock);
    status->busy = atomic_load(&setup_busy);
    status->enabled = setup_web_enabled();
}

/** @brief Gate web configuration independently of the station connection and saved profile. */
esp_err_t probe_wifi_setup_set_enabled(bool enabled)
{
    probe_wifi_setup_status_t status;
    probe_wifi_setup_get_status(&status);
    bool expected = false;
    if (!status.ready || (!enabled && !setup_has_station()) ||
        !atomic_compare_exchange_strong(&setup_busy, &expected, true))
    {
        return ESP_ERR_INVALID_STATE;
    }
    atomic_store(&setup_enabled, enabled);
    atomic_store(&setup_busy, false);
    return ESP_OK;
}

/** @brief Copy scan results without exposing SDK buffers across tasks. */
void probe_wifi_setup_get_scan(probe_wifi_scan_results_t *results)
{
    portENTER_CRITICAL(&setup_lock);
    *results = scan_results;
    portEXIT_CRITICAL(&setup_lock);
}

/** @brief Scan on the worker, deduplicate visible SSIDs and release SDK-owned scan memory. */
static void setup_scan_networks(void)
{
    /* Only this worker owns SDK records; fixed storage avoids increasing its stack use. */
    static wifi_ap_record_t records[PROBE_WIFI_SCAN_MAX];
    probe_wifi_scan_results_t results = {0};
    uint16_t count = PROBE_WIFI_SCAN_MAX;
    esp_err_t result = esp_wifi_scan_start(NULL, true);
    if (result == ESP_OK)
    {
        result = esp_wifi_scan_get_ap_records(&count, records);
    }
    if (result == ESP_OK)
    {
        for (uint16_t index = 0U; index < count; ++index)
        {
            char ssid[33] = {0};
            (void)memcpy(ssid, records[index].ssid, 32U);
            bool duplicate = (ssid[0] == '\0');
            for (uint16_t previous = 0U; previous < results.count; ++previous)
            {
                duplicate = duplicate || (strcmp(ssid, results.entries[previous].ssid) == 0);
            }
            if (!duplicate)
            {
                probe_wifi_scan_entry_t *const entry = &results.entries[results.count];
                (void)memcpy(entry->ssid, ssid, sizeof(ssid));
                entry->rssi = records[index].rssi;
                entry->secured = records[index].authmode != WIFI_AUTH_OPEN;
                ++results.count;
            }
        }
    }
    /* get_ap_records frees the SDK list on success; this also covers every error path. */
    (void)esp_wifi_clear_ap_list();
    results.result = result;
    results.state = (result == ESP_OK) ? PROBE_WIFI_SCAN_DONE : PROBE_WIFI_SCAN_ERROR;
    portENTER_CRITICAL(&setup_lock);
    results.revision = scan_results.revision + 1U;
    scan_results = results;
    setup_status.scan = results.state;
    setup_status.scan_result = result;
    setup_status.scan_count = results.count;
    setup_status.scan_revision = results.revision;
    portEXIT_CRITICAL(&setup_lock);
}

/** @brief Queue a scan only while neither another setup job nor GDB owns the probe. */
esp_err_t probe_wifi_setup_scan(void)
{
    probe_wifi_setup_status_t status;
    probe_wifi_setup_get_status(&status);
    bool expected = false;
    if (!status.ready || !atomic_compare_exchange_strong(&setup_busy, &expected, true))
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (!probe_debug_configuration_lock())
    {
        atomic_store(&setup_busy, false);
        return ESP_ERR_INVALID_STATE;
    }
    portENTER_CRITICAL(&setup_lock);
    scan_results.state = PROBE_WIFI_SCAN_RUNNING;
    setup_status.scan = PROBE_WIFI_SCAN_RUNNING;
    portEXIT_CRITICAL(&setup_lock);
    const setup_job_t job = {.scan = true};
    if (xQueueSend(setup_queue, &job, 0U) != pdTRUE)
    {
        portENTER_CRITICAL(&setup_lock);
        scan_results.state = PROBE_WIFI_SCAN_ERROR;
        scan_results.result = ESP_ERR_TIMEOUT;
        ++scan_results.revision;
        setup_status.scan = PROBE_WIFI_SCAN_ERROR;
        setup_status.scan_result = ESP_ERR_TIMEOUT;
        setup_status.scan_revision = scan_results.revision;
        portEXIT_CRITICAL(&setup_lock);
        probe_debug_configuration_unlock();
        atomic_store(&setup_busy, false);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

/** @brief Apply queued jobs outside HTTP/UI, then erase the local secret copy. */
static void setup_configure_task(void *context)
{
    (void)context;
    for (;;)
    {
        if (!setup_has_station())
        {
            /* Latch recovery mode ON so a new DHCP connection does not close the phone page. */
            atomic_store(&setup_enabled, true);
        }
        setup_job_t job = {0};
        if (xQueueReceive(setup_queue, &job, pdMS_TO_TICKS(250U)) == pdTRUE)
        {
            if (job.scan)
            {
                setup_scan_networks();
            }
            else
            {
                /* Let HTTP acceptance reach the AP client before a channel change. */
                vTaskDelay(pdMS_TO_TICKS(200U));
                const esp_err_t result =
                    probe_network_provision(job.credentials.ssid, job.credentials.password);
                portENTER_CRITICAL(&setup_lock);
                setup_status.configuring = false;
                setup_status.configure_result = result;
                portEXIT_CRITICAL(&setup_lock);
            }
            (void)memset(&job, 0, sizeof(job));
            probe_debug_configuration_unlock();
            atomic_store(&setup_busy, false);
        }
    }
}

/** @brief Accumulate only valid reply round-trip times from the SDK ping task. */
static void setup_ping_success(esp_ping_handle_t handle, void *context)
{
    (void)context;
    uint32_t elapsed = 0U;
    if (esp_ping_get_profile(handle, ESP_PING_PROF_TIMEGAP, &elapsed, sizeof(elapsed)) == ESP_OK)
    {
        portENTER_CRITICAL(&setup_lock);
        ping_total_ms += elapsed;
        portEXIT_CRITICAL(&setup_lock);
    }
}

/** @brief Publish the completed gateway test and retire its finite SDK session. */
static void setup_ping_end(esp_ping_handle_t handle, void *context)
{
    (void)context;
    uint32_t sent = 0U;
    uint32_t received = 0U;
    (void)esp_ping_get_profile(handle, ESP_PING_PROF_REQUEST, &sent, sizeof(sent));
    (void)esp_ping_get_profile(handle, ESP_PING_PROF_REPLY, &received, sizeof(received));
    portENTER_CRITICAL(&setup_lock);
    setup_status.sent = sent;
    setup_status.received = received;
    setup_status.mean_ms = (received > 0U) ? ping_total_ms / received : 0U;
    setup_status.ping = (received > 0U) ? PROBE_WIFI_PING_OK : PROBE_WIFI_PING_NO_REPLY;
    portEXIT_CRITICAL(&setup_lock);
    /* Deletion clears INIT; SDK frees the session after this end callback returns. */
    (void)esp_ping_delete_session(handle);
    atomic_store(&setup_busy, false);
}

/** @brief Start a bounded ping on STA's actual DHCP gateway, never the probe's own AP address. */
esp_err_t probe_wifi_setup_ping_router(void)
{
    probe_wifi_setup_status_t status;
    probe_wifi_setup_get_status(&status);
    if (!status.ready)
    {
        return ESP_ERR_INVALID_STATE;
    }
    bool expected = false;
    if (!atomic_compare_exchange_strong(&setup_busy, &expected, true))
    {
        return ESP_ERR_INVALID_STATE;
    }
    probe_state_t network = {0};
    probe_state_get(&network);
    esp_netif_t *const station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip = {0};
    wifi_ap_record_t router = {0};
    esp_err_t result = ESP_ERR_INVALID_STATE;
    const int interface_index = (station != NULL) ? esp_netif_get_netif_impl_index(station) : 0;
    if (network.station_connected && (station != NULL) && (interface_index > 0) &&
        (esp_wifi_sta_get_ap_info(&router) == ESP_OK) &&
        (esp_netif_get_ip_info(station, &ip) == ESP_OK) && (ip.gw.addr != 0U))
    {
        esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
        config.count = 3U;
        config.interval_ms = 300U;
        config.timeout_ms = 1000U;
        config.data_size = 32U;
        config.task_stack_size = 3072U;
        config.interface = (uint32_t)interface_index;
        ip_addr_set_ip4_u32(&config.target_addr, ip.gw.addr);
        const esp_ping_callbacks_t callbacks = {.on_ping_success = setup_ping_success,
                                                .on_ping_end = setup_ping_end};
        esp_ping_handle_t handle = NULL;
        char gateway[16] = {0};
        (void)snprintf(gateway, sizeof(gateway), IPSTR, IP2STR(&ip.gw));
        portENTER_CRITICAL(&setup_lock);
        setup_status.ping = PROBE_WIFI_PING_RUNNING;
        setup_status.sent = 0U;
        setup_status.received = 0U;
        setup_status.mean_ms = 0U;
        ping_total_ms = 0U;
        (void)memcpy(setup_status.gateway, gateway, sizeof(gateway));
        portEXIT_CRITICAL(&setup_lock);
        result = esp_ping_new_session(&config, &callbacks, &handle);
        if (result == ESP_OK)
        {
            result = esp_ping_start(handle);
            if (result != ESP_OK)
            {
                (void)esp_ping_delete_session(handle);
            }
        }
        if (result != ESP_OK)
        {
            portENTER_CRITICAL(&setup_lock);
            setup_status.ping = PROBE_WIFI_PING_ERROR;
            portEXIT_CRITICAL(&setup_lock);
        }
    }
    else
    {
        portENTER_CRITICAL(&setup_lock);
        setup_status.ping = PROBE_WIFI_PING_NO_WIFI;
        setup_status.gateway[0] = '\0';
        setup_status.sent = 0U;
        setup_status.received = 0U;
        setup_status.mean_ms = 0U;
        portEXIT_CRITICAL(&setup_lock);
    }
    if (result != ESP_OK)
    {
        atomic_store(&setup_busy, false);
    }
    return result;
}

/** @brief Add no-cache headers to portal responses, including credential-free status data. */
static void setup_headers(httpd_req_t *request)
{
    (void)httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    (void)httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
}

/** @brief Send a bounded, static JSON acknowledgement/error response. */
static esp_err_t setup_json(httpd_req_t *request, const char *status, const char *body)
{
    setup_headers(request);
    (void)httpd_resp_set_status(request, status);
    (void)httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, body);
}

/** @brief Require the page's per-boot token on mutating requests; no CORS access is exposed. */
static bool setup_authorized(httpd_req_t *request)
{
    char token[33] = {0};
    return (httpd_req_get_hdr_value_len(request, "X-Probe-Token") == 32U) &&
           (httpd_req_get_hdr_value_str(request, "X-Probe-Token", token, sizeof(token)) ==
            ESP_OK) &&
           (strcmp(token, csrf_token) == 0);
}

/** @brief Serve the self-contained phone page without remote assets or reflected credentials. */
static esp_err_t setup_page(httpd_req_t *request)
{
    setup_headers(request);
    if (!setup_web_enabled())
    {
        (void)httpd_resp_set_status(request, "403 Forbidden");
        (void)httpd_resp_set_type(request, "text/plain; charset=utf-8");
        return httpd_resp_sendstr(
            request, "Wi-Fi setup is OFF. Turn STA setup ON in the OLED WiFi config menu.");
    }
    (void)httpd_resp_set_type(request, "text/html; charset=utf-8");
    (void)httpd_resp_set_hdr(
        request, "Content-Security-Policy",
        "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
        "connect-src 'self'; base-uri 'none'; frame-ancestors 'none'");
    return httpd_resp_sendstr(request, portal_html);
}

/** @brief Return status and the POST token, with all dynamic SSID text escaped as JSON. */
static esp_err_t setup_get_status(httpd_req_t *request)
{
    probe_wifi_setup_status_t status;
    probe_wifi_setup_get_status(&status);
    if (!status.enabled)
    {
        return setup_json(request, "403 Forbidden", "{\"error\":\"setup_disabled\"}");
    }
    probe_state_t network = {0};
    probe_state_get(&network);
    char ssid[200];
    char ap[200];
    char body[1024];
    if (!probe_wifi_json_escape(network.ssid, ssid, sizeof(ssid)) ||
        !probe_wifi_json_escape(network.ap_ssid, ap, sizeof(ap)))
    {
        return setup_json(request, "500 Internal Server Error", "{\"error\":\"status\"}");
    }
    const int length = snprintf(
        body, sizeof(body),
        "{\"ready\":%s,\"busy\":%s,\"configuring\":%s,\"result\":\"%s\","
        "\"connected\":%s,\"ssid\":\"%s\",\"ip\":\"%s\",\"ap\":\"%s\","
        "\"ping\":%u,\"sent\":%" PRIu32 ",\"received\":%" PRIu32 ",\"mean_ms\":%" PRIu32 ","
        "\"gateway\":\"%s\",\"token\":\"%s\",\"scan\":%u,\"scan_revision\":%" PRIu32 "}",
        status.ready ? "true" : "false", status.busy ? "true" : "false",
        status.configuring ? "true" : "false", esp_err_to_name(status.configure_result),
        network.station_connected ? "true" : "false", ssid, network.ip, ap,
        (unsigned int)status.ping, status.sent, status.received, status.mean_ms, status.gateway,
        csrf_token, (unsigned int)status.scan, status.scan_revision);
    if ((length < 0) || ((size_t)length >= sizeof(body)))
    {
        return setup_json(request, "500 Internal Server Error", "{\"error\":\"status\"}");
    }
    return setup_json(request, "200 OK", body);
}

/** @brief Decode credentials, reserve idle debug ownership and queue one asynchronous update. */
static esp_err_t setup_post_wifi(httpd_req_t *request)
{
    probe_wifi_setup_status_t status;
    probe_wifi_setup_get_status(&status);
    if (!status.ready)
    {
        return setup_json(request, "503 Service Unavailable", "{\"error\":\"starting\"}");
    }
    if (!status.enabled)
    {
        return setup_json(request, "403 Forbidden", "{\"error\":\"setup_disabled\"}");
    }
    if (!setup_authorized(request))
    {
        return setup_json(request, "403 Forbidden", "{\"error\":\"token\"}");
    }
    char type[64] = {0};
    if ((httpd_req_get_hdr_value_str(request, "Content-Type", type, sizeof(type)) != ESP_OK) ||
        (strncmp(type, "application/x-www-form-urlencoded", 33U) != 0) ||
        ((type[33] != '\0') && (type[33] != ';')) || (request->content_len == 0U) ||
        (request->content_len > PROBE_WIFI_FORM_MAX))
    {
        (void)httpd_resp_set_hdr(request, "Connection", "close");
        return setup_json(request, "400 Bad Request", "{\"error\":\"form\"}");
    }
    char body[PROBE_WIFI_FORM_MAX + 1U] = {0};
    size_t used = 0U;
    while (used < request->content_len)
    {
        const int received = httpd_req_recv(request, &body[used], request->content_len - used);
        if (received <= 0)
        {
            (void)memset(body, 0, sizeof(body));
            (void)httpd_resp_set_hdr(request, "Connection", "close");
            return setup_json(request, "408 Request Timeout", "{\"error\":\"timeout\"}");
        }
        used += (size_t)received;
    }
    probe_wifi_credentials_t credentials;
    const bool valid = probe_wifi_form_parse(body, used, &credentials);
    (void)memset(body, 0, sizeof(body));
    if (!valid)
    {
        return setup_json(request, "400 Bad Request", "{\"error\":\"credentials\"}");
    }
    bool expected = false;
    if (!atomic_compare_exchange_strong(&setup_busy, &expected, true))
    {
        (void)memset(&credentials, 0, sizeof(credentials));
        return setup_json(request, "409 Conflict", "{\"error\":\"busy\"}");
    }
    if (!probe_debug_configuration_lock())
    {
        atomic_store(&setup_busy, false);
        (void)memset(&credentials, 0, sizeof(credentials));
        return setup_json(request, "409 Conflict", "{\"error\":\"debug_busy\"}");
    }
    portENTER_CRITICAL(&setup_lock);
    setup_status.configuring = true;
    setup_status.configure_result = ESP_OK;
    setup_status.ping = PROBE_WIFI_PING_IDLE;
    setup_status.sent = 0U;
    setup_status.received = 0U;
    setup_status.mean_ms = 0U;
    setup_status.gateway[0] = '\0';
    portEXIT_CRITICAL(&setup_lock);
    setup_job_t job = {.credentials = credentials};
    const bool queued = xQueueSend(setup_queue, &job, 0U) == pdTRUE;
    (void)memset(&job, 0, sizeof(job));
    (void)memset(&credentials, 0, sizeof(credentials));
    if (!queued)
    {
        probe_debug_configuration_unlock();
        atomic_store(&setup_busy, false);
        portENTER_CRITICAL(&setup_lock);
        setup_status.configuring = false;
        setup_status.configure_result = ESP_ERR_TIMEOUT;
        portEXIT_CRITICAL(&setup_lock);
        return setup_json(request, "503 Service Unavailable", "{\"error\":\"queue\"}");
    }
    return setup_json(request, "202 Accepted", "{\"accepted\":true}");
}

/** @brief Start the same gateway diagnostic available from the OLED Ping action. */
static esp_err_t setup_post_ping(httpd_req_t *request)
{
    if (!setup_web_enabled())
    {
        return setup_json(request, "403 Forbidden", "{\"error\":\"setup_disabled\"}");
    }
    if (!setup_authorized(request))
    {
        return setup_json(request, "403 Forbidden", "{\"error\":\"token\"}");
    }
    const esp_err_t result = probe_wifi_setup_ping_router();
    return (result == ESP_OK)
               ? setup_json(request, "202 Accepted", "{\"accepted\":true}")
               : setup_json(request, "409 Conflict", "{\"error\":\"offline_or_busy\"}");
}

/** @brief Return escaped scan entries in bounded chunks, avoiding a large HTTP stack buffer. */
static esp_err_t setup_get_scan(httpd_req_t *request)
{
    if (!setup_web_enabled())
    {
        return setup_json(request, "403 Forbidden", "{\"error\":\"setup_disabled\"}");
    }
    probe_wifi_scan_results_t results;
    probe_wifi_setup_get_scan(&results);
    setup_headers(request);
    (void)httpd_resp_set_type(request, "application/json");
    char body[320];
    (void)snprintf(body, sizeof(body), "{\"state\":%u,\"result\":\"%s\",\"networks\":[",
                   (unsigned int)results.state, esp_err_to_name(results.result));
    esp_err_t result = httpd_resp_sendstr_chunk(request, body);
    for (uint16_t index = 0U; (index < results.count) && (result == ESP_OK); ++index)
    {
        char ssid[200];
        if (!probe_wifi_json_escape(results.entries[index].ssid, ssid, sizeof(ssid)))
        {
            result = ESP_FAIL;
        }
        else
        {
            (void)snprintf(body, sizeof(body), "%s{\"ssid\":\"%s\",\"rssi\":%d,\"secured\":%s}",
                           (index == 0U) ? "" : ",", ssid, (int)results.entries[index].rssi,
                           results.entries[index].secured ? "true" : "false");
            result = httpd_resp_sendstr_chunk(request, body);
        }
    }
    if (result == ESP_OK)
    {
        result = httpd_resp_sendstr_chunk(request, "]}");
    }
    return (result == ESP_OK) ? httpd_resp_send_chunk(request, NULL, 0) : result;
}

/** @brief Start the scan worker through the same token-protected portal origin. */
static esp_err_t setup_post_scan(httpd_req_t *request)
{
    if (!setup_web_enabled())
    {
        return setup_json(request, "403 Forbidden", "{\"error\":\"setup_disabled\"}");
    }
    if (!setup_authorized(request))
    {
        return setup_json(request, "403 Forbidden", "{\"error\":\"token\"}");
    }
    return (probe_wifi_setup_scan() == ESP_OK)
               ? setup_json(request, "202 Accepted", "{\"accepted\":true}")
               : setup_json(request, "409 Conflict", "{\"error\":\"busy_or_debug\"}");
}

/** @brief Bind the server to the AP interface and clean up every partial startup stage. */
esp_err_t probe_wifi_setup_start(void)
{
    if ((setup_queue != NULL) || (setup_server != NULL))
    {
        return ESP_ERR_INVALID_STATE;
    }
    (void)snprintf(csrf_token, sizeof(csrf_token),
                   "%08" PRIx32 "%08" PRIx32 "%08" PRIx32 "%08" PRIx32, esp_random(), esp_random(),
                   esp_random(), esp_random());
    setup_queue = xQueueCreate(1U, sizeof(setup_job_t));
    esp_err_t result = (setup_queue != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
    if (result == ESP_OK)
    {
        result = (xTaskCreatePinnedToCore(setup_configure_task, "wifi_setup", 4096U, NULL, 2U,
                                          &setup_worker, 0) == pdPASS)
                     ? ESP_OK
                     : ESP_ERR_NO_MEM;
    }
    if (result == ESP_OK)
    {
        struct ifreq interface = {0};
        esp_netif_t *const ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        result = (ap != NULL) ? esp_netif_get_netif_impl_name(ap, interface.ifr_name)
                              : ESP_ERR_INVALID_STATE;
        if (result == ESP_OK)
        {
            httpd_config_t config = HTTPD_DEFAULT_CONFIG();
            config.stack_size = 8192U;
            config.core_id = 0;
            config.max_open_sockets = 3U;
            config.max_uri_handlers = 6U;
            config.recv_wait_timeout = 3U;
            config.send_wait_timeout = 3U;
            config.lru_purge_enable = true;
            config.if_name = &interface;
            result = httpd_start(&setup_server, &config);
        }
    }
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = setup_page},
        {.uri = "/api/status", .method = HTTP_GET, .handler = setup_get_status},
        {.uri = "/api/wifi", .method = HTTP_POST, .handler = setup_post_wifi},
        {.uri = "/api/ping", .method = HTTP_POST, .handler = setup_post_ping},
        {.uri = "/api/scan", .method = HTTP_GET, .handler = setup_get_scan},
        {.uri = "/api/scan", .method = HTTP_POST, .handler = setup_post_scan}};
    for (size_t index = 0U; (index < sizeof(routes) / sizeof(routes[0])) && (result == ESP_OK);
         ++index)
    {
        result = httpd_register_uri_handler(setup_server, &routes[index]);
    }
    if (result != ESP_OK)
    {
        if (setup_server != NULL)
        {
            (void)httpd_stop(setup_server);
            setup_server = NULL;
        }
        if (setup_worker != NULL)
        {
            vTaskDelete(setup_worker);
            setup_worker = NULL;
        }
        if (setup_queue != NULL)
        {
            vQueueDelete(setup_queue);
            setup_queue = NULL;
        }
    }
    portENTER_CRITICAL(&setup_lock);
    setup_status.ready = result == ESP_OK;
    portEXIT_CRITICAL(&setup_lock);
    if (result == ESP_OK)
    {
        ESP_LOGI("probe_wifi_setup", "Web setup ready; no STA keeps setup ON");
    }
    return result;
}
