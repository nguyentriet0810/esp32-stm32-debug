/*
 * SPDX-FileCopyrightText: 2026 Piotr Wasilewski
 * SPDX-FileCopyrightText: 2011 Black Sphere Technologies Ltd.
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 * TCP/GDB poll loop adapted from MCUViewer-probe at 02755f4.
 */
/** @file probe_debug.c @brief Single-owner core-1 backend and core-0 TCP acceptor. */
#include "probe_debug.h"
#include <errno.h>
#include <limits.h>
#include <stdatomic.h>
#include <stddef.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "sdkconfig.h"
#include "probe_state.h"
#include "probe_port.h"
#include "general.h"
#include "exception.h"
#include "gdb_if.h"
#include "gdb_main.h"
#include "gdb_packet.h"
#include "target_internal.h"
typedef struct
{
    int socket;
    uint32_t clock_hz;
} debug_command_t;
static QueueHandle_t command_queue;
static QueueHandle_t startup_queue;
static atomic_bool debug_busy;
static int client_socket = -1;
static bool eot_pending;
static uint8_t rx_buffer[1024];
static size_t rx_length;
static size_t rx_index;
static uint8_t tx_buffer[1024];
static size_t tx_length;
static void debug_disconnect(void)
{
    if (client_socket >= 0)
    {
        (void)shutdown(client_socket, SHUT_RDWR);
        (void)close(client_socket);
    }
    client_socket = -1;
    rx_length = 0U;
    rx_index = 0U;
    tx_length = 0U;
    eot_pending = true;
}
static bool debug_receive(uint32_t timeout_ms)
{
    bool received = false;
    if (client_socket >= 0)
    {
        fd_set sockets;
        FD_ZERO(&sockets);
        FD_SET(client_socket, &sockets);
        struct timeval timeout = {.tv_sec = (time_t)(timeout_ms / 1000U),
                                  .tv_usec = (suseconds_t)((timeout_ms % 1000U) * 1000U)};
        const int ready = select(client_socket + 1, &sockets, NULL, NULL, &timeout);
        if (ready > 0)
        {
            const ssize_t count = recv(client_socket, rx_buffer, sizeof(rx_buffer), 0);
            if (count > 0)
            {
                rx_length = (size_t)count;
                rx_index = 0U;
                received = true;
            }
            else
            {
                debug_disconnect();
            }
        }
        else if ((ready < 0) && (errno != EINTR))
        {
            debug_disconnect();
        }
    }
    return received;
}
int gdb_if_init(void)
{
    return 0;
}
char gdb_if_getchar(void)
{
    while ((!eot_pending) && (rx_index >= rx_length) && (client_socket >= 0))
    {
        (void)debug_receive(1000U);
    }
    char value = '\x04';
    if (eot_pending)
    {
        eot_pending = false;
    }
    else if (rx_index < rx_length)
    {
        value = (char)rx_buffer[rx_index++];
    }
    return value;
}
char gdb_if_getchar_to(uint32_t timeout)
{
    char value = (char)-1;
    if (eot_pending)
    {
        value = '\x04';
    }
    else
    {
        if ((rx_index >= rx_length) && (client_socket >= 0))
        {
            (void)debug_receive((timeout > (uint32_t)INT32_MAX) ? (uint32_t)INT32_MAX : timeout);
        }
        if (eot_pending)
        {
            value = '\x04';
        }
        else if (rx_index < rx_length)
        {
            value = (char)rx_buffer[rx_index++];
        }
    }
    return value;
}
void gdb_if_flush(bool force)
{
    (void)force;
    size_t sent = 0U;
    while ((client_socket >= 0) && (sent < tx_length))
    {
        const ssize_t count = send(client_socket, &tx_buffer[sent], tx_length - sent, 0);
        if (count > 0)
        {
            sent += (size_t)count;
        }
        else if ((count == 0) || (errno != EINTR))
        {
            debug_disconnect();
        }
    }
    tx_length = 0U;
}
void gdb_if_putchar(char value, bool flush)
{
    if (client_socket >= 0)
    {
        tx_buffer[tx_length++] = (uint8_t)value;
        if (flush || (tx_length == sizeof(tx_buffer)))
        {
            gdb_if_flush(false);
        }
    }
}
static void debug_poll(void)
{
    while (gdb_target_running && (cur_target != NULL))
    {
        gdb_poll_target();
        if (gdb_target_running && (cur_target != NULL))
        {
            const char command = gdb_if_getchar_to(0U);
            if ((command == '\x03') || (command == '\x04'))
            {
                target_halt_request(cur_target);
            }
            vTaskDelay(1U);
        }
    }
    const gdb_packet_s *packet = gdb_packet_receive();
    gdb_main(packet);
    probe_state_debug(client_socket >= 0, gdb_target_running,
                      (cur_target != NULL) ? cur_target->driver : "");
}
static void debug_owner(void *context)
{
    (void)context;
    esp_err_t result = probe_port_init();
#if CONFIG_PROBE_NRST_GPIO >= 0
    if (result == ESP_OK)
    {
        (void)gpio_set_level((gpio_num_t)CONFIG_PROBE_NRST_GPIO, 1U);
        const gpio_config_t reset = {.pin_bit_mask = UINT64_C(1) << CONFIG_PROBE_NRST_GPIO,
                                     .mode = GPIO_MODE_INPUT_OUTPUT_OD,
                                     .pull_up_en = GPIO_PULLUP_ENABLE,
                                     .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                     .intr_type = GPIO_INTR_DISABLE};
        result = gpio_config(&reset);
    }
#endif
    /* Publish the real GPIO initialization result before the listener may start. */
    (void)xQueueSend(startup_queue, &result, portMAX_DELAY);
    if (result != ESP_OK)
    {
        ESP_LOGE("probe_debug", "SWD init failed: %s", esp_err_to_name(result));
        probe_state_error();
        vTaskDelete(NULL);
    }
    probe_state_clock(probe_swd_get_frequency());
    for (;;)
    {
        debug_command_t command = {.socket = -1, .clock_hz = 0U};
        if (xQueueReceive(command_queue, &command, portMAX_DELAY) == pdTRUE)
        {
            if (command.socket < 0)
            {
                platform_max_frequency_set(command.clock_hz);
            }
            else
            {
                client_socket = command.socket;
                rx_length = 0U;
                rx_index = 0U;
                tx_length = 0U;
                eot_pending = false;
                gdb_set_noackmode(false);
                probe_state_debug(true, false, "");
                ESP_LOGI("probe_debug", "GDB session opened");
                while ((client_socket >= 0) || eot_pending)
                {
                    /* BMD-001: upstream non-local recovery, confined to this owner. */
                    TRY(EXCEPTION_ALL)
                    {
                        debug_poll();
                    }
                    CATCH()
                    {
                    default:
                        gdb_put_packet_error(0xffU);
                        target_list_free();
                        cur_target = NULL;
                        gdb_target_running = false;
                        probe_state_error();
                        ESP_LOGW("probe_debug", "Target error recovered");
                        break;
                    }
                }
                target_list_free();
                cur_target = NULL;
                gdb_target_running = false;
                probe_swd_drive(false);
                probe_state_debug(false, false, "");
                atomic_store(&debug_busy, false);
                ESP_LOGI("probe_debug", "GDB session closed");
            }
        }
    }
}
static void debug_listener(void *context)
{
    (void)context;
    int listener = -1;
    while (listener < 0)
    {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener >= 0)
        {
            const int one = 1;
            (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
            const struct sockaddr_in address = {.sin_family = AF_INET,
                                                .sin_port = htons(CONFIG_PROBE_GDB_PORT),
                                                .sin_addr.s_addr = htonl(INADDR_ANY)};
            if ((bind(listener, (const struct sockaddr *)&address, sizeof(address)) != 0) ||
                (listen(listener, 2) != 0))
            {
                (void)close(listener);
                listener = -1;
            }
        }
        if (listener < 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1000U));
        }
    }
    ESP_LOGI("probe_debug", "GDB ready on TCP %d", CONFIG_PROBE_GDB_PORT);
    for (;;)
    {
        const int socket_fd = accept(listener, NULL, NULL);
        if (socket_fd >= 0)
        {
            bool expected = false;
            if (atomic_compare_exchange_strong(&debug_busy, &expected, true))
            {
                const int one = 1;
                const int idle = 5;
                const int interval = 2;
                const int count = 3;
                const struct timeval send_timeout = {.tv_sec = 5, .tv_usec = 0};
                (void)setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
                (void)setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
                (void)setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
                (void)setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval,
                                 sizeof(interval));
                (void)setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
                (void)setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout,
                                 sizeof(send_timeout));
                const debug_command_t command = {.socket = socket_fd, .clock_hz = 0U};
                if (xQueueSend(command_queue, &command, 0U) != pdTRUE)
                {
                    (void)close(socket_fd);
                    atomic_store(&debug_busy, false);
                }
            }
            else
            {
                (void)close(socket_fd);
                ESP_LOGW("probe_debug", "Second client rejected: busy");
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100U));
        }
    }
}
esp_err_t probe_debug_start(void)
{
    command_queue = xQueueCreate(4U, sizeof(debug_command_t));
    startup_queue = xQueueCreate(1U, sizeof(esp_err_t));
    esp_err_t result =
        ((command_queue == NULL) || (startup_queue == NULL)) ? ESP_ERR_NO_MEM : ESP_OK;
    if (result == ESP_OK)
    {
        if (xTaskCreatePinnedToCore(debug_owner, "swd_owner", 16384U, NULL, 5U, NULL, 1) != pdPASS)
        {
            result = ESP_ERR_NO_MEM;
        }
    }
    if (result == ESP_OK)
    {
        if (xQueueReceive(startup_queue, &result, pdMS_TO_TICKS(5000U)) != pdTRUE)
        {
            result = ESP_ERR_TIMEOUT;
        }
    }
    if (result == ESP_OK)
    {
        if (xTaskCreatePinnedToCore(debug_listener, "gdb_accept", 4096U, NULL, 3U, NULL, 0) !=
            pdPASS)
        {
            result = ESP_ERR_NO_MEM;
        }
    }
    return result;
}
esp_err_t probe_debug_set_clock(uint32_t hz)
{
    esp_err_t result = ESP_ERR_INVALID_ARG;
    if ((hz >= 1000U) && (hz <= 8000000U) && (command_queue != NULL))
    {
        if (atomic_load(&debug_busy))
        {
            result = ESP_ERR_INVALID_STATE;
        }
        else
        {
            const debug_command_t command = {.socket = -1, .clock_hz = hz};
            result = (xQueueSend(command_queue, &command, 0U) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
        }
    }
    return result;
}
bool probe_debug_configuration_lock(void)
{
    bool expected = false;
    return atomic_compare_exchange_strong(&debug_busy, &expected, true);
}
void probe_debug_configuration_unlock(void)
{
    atomic_store(&debug_busy, false);
}
