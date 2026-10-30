/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_port.h @brief Private interface; only the core-1 owner accesses GPIO. */
#ifndef PROBE_PORT_H
#define PROBE_PORT_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
esp_err_t probe_port_init(void);
void probe_swd_set_frequency(uint32_t hz);
uint32_t probe_swd_get_frequency(void);
void probe_swd_drive(bool enable);
#endif
