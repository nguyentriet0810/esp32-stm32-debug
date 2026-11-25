/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file sdkconfig.h @brief PCB OLED pins and compile-time feature variants for host tests. */
#ifndef OLED_TEST_SDKCONFIG_H
#define OLED_TEST_SDKCONFIG_H
#ifndef CONFIG_PROBE_OLED_ENABLE
#define CONFIG_PROBE_OLED_ENABLE 1
#endif
#ifndef CONFIG_PROBE_OLED_COLUMN_OFFSET
#define CONFIG_PROBE_OLED_COLUMN_OFFSET 2
#endif
#define CONFIG_PROBE_OLED_SDA_GPIO 8
#define CONFIG_PROBE_OLED_SCL_GPIO 9
#endif
