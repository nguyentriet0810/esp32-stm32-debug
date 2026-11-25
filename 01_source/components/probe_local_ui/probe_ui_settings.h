/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_ui_settings.h
 * @brief Private, platform-independent state for the provisional 16-column settings menu.
 *
 * Values are UI examples stored in RAM only. No debug, display or persistent settings are
 * changed by this model. One UI task owns each instance; the API is not ISR-safe.
 */
#ifndef PROBE_UI_SETTINGS_H
#define PROBE_UI_SETTINGS_H
#include <stdbool.h>
#include <stdint.h>

#define PROBE_UI_SETTINGS_ROWS 8U
#define PROBE_UI_SETTINGS_COLUMNS 16U
#define PROBE_UI_SETTINGS_ITEMS (PROBE_UI_SETTINGS_ROWS - 1U)

/** @brief Actions mapped from debounced encoder events by the UI adapter. */
typedef enum
{
    PROBE_UI_SETTINGS_PREVIOUS,
    PROBE_UI_SETTINGS_NEXT,
    PROBE_UI_SETTINGS_SELECT,
    PROBE_UI_SETTINGS_CANCEL
} probe_ui_settings_action_t;

/** @brief Caller-owned menu state; fields are private to the model after initialization. */
typedef struct
{
    uint8_t selected;                        /**< Item index, 0..6; excludes the header. */
    uint8_t values[PROBE_UI_SETTINGS_ITEMS]; /**< Committed example choice indices. */
    uint8_t draft;                           /**< Pending choice for the selected item. */
    bool editing;                            /**< Rotation changes draft while true. */
} probe_ui_settings_t;

/**
 * @brief Initialize the first selectable row and all provisional values.
 * @param[out] settings Non-NULL caller-owned model, retained by the caller.
 * @note Task context only; does not allocate, perform I/O or access persistent storage.
 */
void probe_ui_settings_init(probe_ui_settings_t *settings);
/**
 * @brief Navigate cyclically, edit a bounded choice, commit, or cancel an edit.
 * @param[in,out] settings Non-NULL initialized model owned by the calling UI task.
 * @param[in] action One encoder action; unknown actions are ignored.
 * @return True if visible state changed and a redraw is needed.
 * @note Navigation skips the header. Editing stays on the selected row and clamps at the
 *       first/last choice. Cancel discards only the draft; select commits to RAM only.
 */
bool probe_ui_settings_input(probe_ui_settings_t *settings, probe_ui_settings_action_t action);
/**
 * @brief Compose eight terminated rows with a fixed header and one '>' cursor.
 * @param[in] settings Non-NULL initialized model, borrowed until return.
 * @param[out] lines Caller-owned 8x17-byte buffer; each row contains 16 characters plus NUL.
 * @note Task context only. Brackets enclose the selected draft while editing. No I/O occurs.
 */
void probe_ui_settings_render(const probe_ui_settings_t *settings,
                              char lines[PROBE_UI_SETTINGS_ROWS][PROBE_UI_SETTINGS_COLUMNS + 1U]);
#endif
