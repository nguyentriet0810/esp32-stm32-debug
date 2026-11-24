/*
 * SPDX-FileCopyrightText: 2026 Piotr Wasilewski
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Black Magic Debug platform definitions for the ESP32-S3 port.
 *
 * BMD includes this header from all of its sources. It deliberately does not include any
 * ESP-IDF header: BMD uses CONFIG_* macros of its own, which must not meet sdkconfig.h.
 */

/**
 * @file platform.h
 * @brief Platform identity and status hooks visible to upstream BMD.
 *
 * Keep this header free of ESP-IDF headers: upstream CONFIG_* names must remain isolated from
 * sdkconfig.h. Status-state macros intentionally perform no hardware I/O.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "timing.h"

#define PLATFORM_IDENT "(NETWORK PROBE ESP32-S3) "

/* The local UI reports status; these upstream LED macros deliberately perform no I/O. */
#define SET_RUN_STATE(state)                                                                       \
    do                                                                                             \
    {                                                                                              \
        (void)(state);                                                                             \
    } while (0)
#define SET_IDLE_STATE(state)                                                                      \
    do                                                                                             \
    {                                                                                              \
        (void)(state);                                                                             \
    } while (0)
#define SET_ERROR_STATE(state)                                                                     \
    do                                                                                             \
    {                                                                                              \
        (void)(state);                                                                             \
    } while (0)

#endif /* PLATFORM_H */
