/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_ui_settings.h
 * @brief Private, platform-independent Settings home screen and child-page model.
 *
 * Flash/Target/Link choices describe UI intent only; they do not activate a transport or
 * programming backend. One UI task owns the model and applies the OLED contrast output.
 * Settings are volatile and the API performs no I/O, allocation or persistent writes.
 */
#ifndef PROBE_UI_SETTINGS_H
#define PROBE_UI_SETTINGS_H
#include <stdbool.h>
#include <stdint.h>

#define PROBE_UI_SETTINGS_ROWS 8U
#define PROBE_UI_SETTINGS_COLUMNS 16U
#define PROBE_UI_SETTINGS_ITEMS 5U
#define PROBE_UI_SETTINGS_VALUES 4U

/** @brief Actions mapped from debounced encoder events by the UI adapter. */
typedef enum
{
    PROBE_UI_SETTINGS_PREVIOUS,
    PROBE_UI_SETTINGS_NEXT,
    PROBE_UI_SETTINGS_SELECT,
    PROBE_UI_SETTINGS_CANCEL
} probe_ui_settings_action_t;

/** @brief Home page followed by one child page per home item, in display order. */
typedef enum
{
    PROBE_UI_SETTINGS_HOME,
    PROBE_UI_SETTINGS_FLASH,
    PROBE_UI_SETTINGS_TARGET,
    PROBE_UI_SETTINGS_LINK,
    PROBE_UI_SETTINGS_OLED,
    PROBE_UI_SETTINGS_INFO
} probe_ui_settings_page_t;

/** @brief Caller-owned state; fields are private to the model after initialization. */
typedef struct
{
    probe_ui_settings_page_t page;            /**< Current home or selection page. */
    uint8_t selected;                         /**< Remembered home cursor, 0..4, including Info. */
    uint8_t values[PROBE_UI_SETTINGS_VALUES]; /**< Committed Flash/Target/Link/OLED indices. */
    uint8_t choice;                           /**< Uncommitted choice in the child page. */
} probe_ui_settings_t;

/**
 * @brief Start on Flash with REMOTE, SWD, WiFi and 50 percent contrast selected.
 * @param[out] settings Non-NULL caller-owned model, retained by the caller.
 * @note Task context only; no hardware or persistent configuration is changed.
 */
void probe_ui_settings_init(probe_ui_settings_t *settings);
/**
 * @brief Navigate home or a child, open a child, commit a choice, or cancel and return.
 * @param[in,out] settings Non-NULL initialized model owned by the calling UI task.
 * @param[in] action One encoder action; unknown actions are ignored.
 * @return True if visible state changed and a redraw is needed.
 * @note Home navigation wraps and skips locked items; SDCARD permits Flash, OLED and Info.
 *       Selecting a home row opens a child immediately. In a child, select commits to RAM
 *       and returns home; cancel returns without committing. OLED choices clamp at their
 *       endpoints and provide a live contrast preview. Other choice lists wrap. Info is
 *       read-only; select/cancel return home, and rotation is ignored.
 */
bool probe_ui_settings_input(probe_ui_settings_t *settings, probe_ui_settings_action_t action);
/**
 * @brief Compose eight terminated 16-character rows for the current page.
 * @param[in] settings Non-NULL initialized model, borrowed until return.
 * @param[out] lines Caller-owned 8x17-byte buffer, consumed before return.
 * @note Task context only. Home shows four settings plus Info; blank rows separate the
 *       header, settings and final Info entry. Locked rows have '--'
 *       and cannot receive the cursor. In child pages, '>' selects a draft and '*' marks
 *       the committed choice. Info displays board name, author, copyright year and license.
 */
void probe_ui_settings_render(const probe_ui_settings_t *settings,
                              char lines[PROBE_UI_SETTINGS_ROWS][PROBE_UI_SETTINGS_COLUMNS + 1U]);
/**
 * @brief Read the OLED contrast percentage, including the uncommitted OLED-page preview.
 * @param[in] settings Non-NULL initialized model, borrowed until return.
 * @return One of 10, 25, 50, 75 or 100 percent; cancel restores the committed value.
 * @note Pure task-context query; the display-owning adapter applies this value to the OLED.
 */
uint8_t probe_ui_settings_oled_percent(const probe_ui_settings_t *settings);
#endif
