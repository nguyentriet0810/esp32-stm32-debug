/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_ui_settings.h
 * @brief Private, platform-independent Settings home screen and child-page model.
 *
 * Flash/Target/Link choices describe UI intent only; they do not activate a transport or
 * programming backend. One UI task owns the model, applies OLED contrast and handles
 * one-shot Wi-Fi setup requests. Settings are volatile; this pure API performs no I/O,
 * allocation or persistent writes. AP access data is copied only while its page is open.
 */
#ifndef PROBE_UI_SETTINGS_H
#define PROBE_UI_SETTINGS_H
#include <stdbool.h>
#include <stdint.h>

#define PROBE_UI_SETTINGS_ROWS 8U
#define PROBE_UI_SETTINGS_COLUMNS 16U
#define PROBE_UI_SETTINGS_ITEMS 5U
#define PROBE_UI_SETTINGS_VALUES 4U
#define PROBE_UI_SWD_ITEMS 4U
#define PROBE_UI_SWD_ENTRIES (PROBE_UI_SWD_ITEMS + 1U)

/** @brief Actions mapped from debounced encoder events by the UI adapter. */
typedef enum
{
    PROBE_UI_SETTINGS_PREVIOUS,
    PROBE_UI_SETTINGS_NEXT,
    PROBE_UI_SETTINGS_SELECT,
    PROBE_UI_SETTINGS_CANCEL
} probe_ui_settings_action_t;

/** @brief Home, its five child pages, and nested SWD/Wi-Fi settings pages. */
typedef enum
{
    PROBE_UI_SETTINGS_HOME,
    PROBE_UI_SETTINGS_FLASH,
    PROBE_UI_SETTINGS_TARGET,
    PROBE_UI_SETTINGS_LINK,
    PROBE_UI_SETTINGS_OLED,
    PROBE_UI_SETTINGS_INFO,
    PROBE_UI_SETTINGS_SWD,
    PROBE_UI_SETTINGS_WIFI
} probe_ui_settings_page_t;

/** @brief UI copy of AP access data and credential-free live network status. */
typedef struct
{
    char station_ssid[33];
    char ap_ssid[33];
    char ap_password[64];
    bool portal_ready;
    bool setup_enabled;
    bool station_connected;
    bool busy;
} probe_ui_wifi_view_t;

/** @brief Committed UI intent for the SWD backend; no hardware is changed by reading it. */
typedef struct
{
    uint32_t frequency_hz;   /**< Nominal SWD frequency, in Hz. */
    uint16_t reset_pulse_ms; /**< Requested nRST assertion duration, in milliseconds. */
    bool use_nrst;           /**< Allow target reset through the probe's nRST pin. */
    bool use_swo;            /**< Request SWO capture when its backend is implemented. */
} probe_ui_swd_config_t;

/** @brief Caller-owned state; fields are private to the model after initialization. */
typedef struct
{
    probe_ui_settings_page_t page;            /**< Current home or selection page. */
    uint8_t selected;                         /**< Remembered home cursor, 0..4, including Info. */
    uint8_t values[PROBE_UI_SETTINGS_VALUES]; /**< Committed Flash/Target/Link/OLED indices. */
    uint8_t choice;                           /**< Child choice, or its count for the Back row. */
    uint8_t swd_selected;                     /**< SWD cursor: frequency, nRST, pulse, SWO, Back. */
    uint8_t swd_values[PROBE_UI_SWD_ITEMS];   /**< Committed SWD choice indices. */
    uint8_t swd_draft;                        /**< Uncommitted choice in the selected SWD field. */
    bool swd_editing;                         /**< Rotation edits a draft instead of navigating. */
    probe_ui_wifi_view_t wifi;                /**< AP secret is retained only on the Wi-Fi page. */
    uint8_t wifi_selected;                    /**< STA setup or Back. */
    bool wifi_setup_requested;                /**< One pending web-mode toggle request. */
} probe_ui_settings_t;

/**
 * @brief Start on Flash with REMOTE, SWD, WiFi and 50 percent contrast selected.
 * @param[out] settings Non-NULL caller-owned model, retained by the caller.
 * @note SWD defaults: 400 kHz, nRST enabled, 100 ms reset pulse, SWO disabled.
 *       Task context only; no hardware or persistent configuration is changed.
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
 *       endpoints (including Back) and provide a live contrast preview. Other lists wrap
 *       through Back. Back cancels a pending choice; on OLED it restores the committed level.
 *       Info has a single Back action, and rotation is ignored.
 *       Selecting SWD in Target opens its settings page. There, select enters/commits an edit,
 *       cancel discards a draft or returns to Target. The Back row also returns to Target.
 *       Reset width is locked when nRST is off.
 *       Selecting WiFi in Link opens STA identity, setup ON/OFF, AP access and Back.
 *       Without an active STA name, setup stays ON and cannot be switched OFF.
 *       Back clears the AP view and returns to Link; the adapter consumes requests separately.
 */
bool probe_ui_settings_input(probe_ui_settings_t *settings, probe_ui_settings_action_t action);
/**
 * @brief Compose eight terminated 16-character rows for the current page.
 * @param[in] settings Non-NULL initialized model, borrowed until return.
 * @param[out] lines Caller-owned 8x17-byte buffer, consumed before return.
 * @note Task context only. Home shows four settings plus Info; blank rows separate the
 *       header, settings and final Info entry. Locked rows have '--'
 *       and cannot receive the cursor. In child pages, '>' selects a draft and '*' marks
 *       the committed choice. Every child has a final-row Back action; home has none.
 *       Info displays board name, author, copyright year and license above its Back action.
 *       Every page leaves row 1 blank; Wi-Fi hides IP/AP/PW rows when setup is OFF.
 */
void probe_ui_settings_render(const probe_ui_settings_t *settings,
                              char lines[PROBE_UI_SETTINGS_ROWS][PROBE_UI_SETTINGS_COLUMNS + 1U]);
/**
 * @brief Read the OLED contrast percentage, including the uncommitted OLED-page preview.
 * @param[in] settings Non-NULL initialized model, borrowed until return.
 * @return One of 10, 25, 50, 75 or 100 percent; cancel/Back restore the committed value.
 * @note Pure task-context query; the display-owning adapter applies this value to the OLED.
 */
uint8_t probe_ui_settings_oled_percent(const probe_ui_settings_t *settings);
/**
 * @brief Copy committed SWD UI choices in explicit units for future backend integration.
 * @param[in] settings Non-NULL initialized model, borrowed until return.
 * @param[out] config Non-NULL caller-owned output; no draft or pointer is retained.
 * @note Single-owner task context only. Reads RAM; does not configure GPIO, queue a clock
 *       change, pulse reset or start trace capture. Pulse width is retained while nRST is off.
 */
void probe_ui_settings_swd_get(const probe_ui_settings_t *settings, probe_ui_swd_config_t *config);
/**
 * @brief Replace the live Wi-Fi page snapshot, returning whether the display changed.
 * @param[in,out] settings Initialized UI-owned model.
 * @param[in] view Borrowed snapshot with terminated strings; copied before return.
 * @return True on a changed Wi-Fi view; false on another page or identical data.
 * @note Credentials are for the probe AP, never the router. UI task context only.
 */
bool probe_ui_settings_wifi_update(probe_ui_settings_t *settings, const probe_ui_wifi_view_t *view);
/**
 * @brief Consume a requested web-mode change; this does not toggle the STA connection.
 * @param[in,out] settings Initialized UI-owned model.
 * @param[out] enabled Non-NULL destination for the requested web-mode state.
 * @return True once for a pending toggle; false without changing enabled otherwise.
 * @note The adapter applies the request and refreshes the actual result through wifi_update.
 */
bool probe_ui_settings_wifi_setup_take(probe_ui_settings_t *settings, bool *enabled);
#endif
