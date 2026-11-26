/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_ui_settings.c
 * @brief Settings home, bounded child choices, copyright information and SD-mode row locks.
 */
#include "probe_ui_settings.h"
#include <stddef.h>
#include <string.h>

/** @brief Item labels fit six cells; each choice fits the seven-cell home value field. */
typedef struct
{
    char label[7];
    char title[13];
    char choices[5][8];
    uint8_t count;
    uint8_t initial;
} settings_item_t;

static const settings_item_t items[PROBE_UI_SETTINGS_VALUES] = {
    {"Flash", "Flash source", {"REMOTE", "SDCARD"}, 2U, 0U},
    {"Target", "Target IF", {"SWD", "UART", "I2C", "SPI"}, 4U, 0U},
    {"Link", "Host link", {"WiFi", "BLE", "ESP-NOW"}, 3U, 0U},
    {"OLED %", "OLED", {"10", "25", "50", "75", "100"}, 5U, 2U}};
static const uint8_t oled_percentages[] = {10U, 25U, 50U, 75U, 100U};

/** @brief SWD detail fields in visible order; pulse width depends on the nRST setting. */
enum
{
    SWD_FREQUENCY,
    SWD_NRST,
    SWD_RESET_PULSE,
    SWD_SWO
};

/** @brief Bounded UI choices; values fit the four-character SWD detail field. */
typedef struct
{
    char label[9];
    char choices[8][5];
    uint8_t count;
    uint8_t initial;
} swd_item_t;

static const swd_item_t swd_items[PROBE_UI_SWD_ITEMS] = {
    {"Freq kHz", {"100", "400", "1000", "2000", "4000", "8000"}, 6U, 1U},
    {"NRST", {"OFF", "ON"}, 2U, 1U},
    {"Reset ms", {"1", "5", "10", "20", "50", "100", "200", "500"}, 8U, 5U},
    {"SWO", {"OFF", "ON"}, 2U, 0U}};
static const uint32_t swd_frequencies_hz[] = {100000U,  400000U,  1000000U,
                                              2000000U, 4000000U, 8000000U};
static const uint16_t swd_reset_pulses_ms[] = {1U, 5U, 10U, 20U, 50U, 100U, 200U, 500U};

/**
 * @brief Determine whether a home row may be selected.
 * @param[in] settings Initialized model.
 * @param[in] item Home row index, 0..4, including the read-only Info entry.
 * @return True for every REMOTE row, or Flash, OLED and Info in SDCARD mode.
 */
static bool settings_enabled(const probe_ui_settings_t *settings, size_t item)
{
    return (settings->values[0] == 0U) || (item == 0U) || (item == 3U) || (item == 4U);
}

/** @brief Move the home cursor cyclically, respecting all mode-dependent row locks. */
static bool settings_move_home(probe_ui_settings_t *settings, bool next)
{
    uint8_t candidate = settings->selected;
    for (size_t attempt = 0U; attempt < PROBE_UI_SETTINGS_ITEMS; ++attempt)
    {
        candidate = next ? (uint8_t)((candidate + 1U) % PROBE_UI_SETTINGS_ITEMS)
                         : ((candidate == 0U) ? (uint8_t)(PROBE_UI_SETTINGS_ITEMS - 1U)
                                              : (uint8_t)(candidate - 1U));
        if (settings_enabled(settings, candidate))
        {
            break;
        }
    }
    const bool changed = (candidate != settings->selected);
    settings->selected = candidate;
    return changed;
}

/** @brief Move through child choices plus Back; OLED clamps while other lists wrap. */
static bool settings_move_choice(probe_ui_settings_t *settings, bool next)
{
    const size_t item = (size_t)settings->page - 1U;
    const uint8_t count = (uint8_t)(items[item].count + 1U);
    const uint8_t previous = settings->choice;
    if (settings->page == PROBE_UI_SETTINGS_OLED)
    {
        if (next && ((settings->choice + 1U) < count))
        {
            ++settings->choice;
        }
        else if (!next && (settings->choice > 0U))
        {
            --settings->choice;
        }
    }
    else
    {
        settings->choice = next ? (uint8_t)((settings->choice + 1U) % count)
                                : ((settings->choice == 0U) ? (uint8_t)(count - 1U)
                                                            : (uint8_t)(settings->choice - 1U));
    }
    return settings->choice != previous;
}

/** @brief Permit every SWD field except reset width while the committed nRST choice is off. */
static bool settings_swd_enabled(const probe_ui_settings_t *settings, size_t field)
{
    return (field != SWD_RESET_PULSE) || (settings->swd_values[SWD_NRST] != 0U);
}

/** @brief Move a bounded SWD draft or cycle through enabled detail rows. */
static bool settings_swd_move(probe_ui_settings_t *settings, bool next)
{
    bool changed = false;
    if (settings->swd_editing)
    {
        if (next && ((settings->swd_draft + 1U) < swd_items[settings->swd_selected].count))
        {
            ++settings->swd_draft;
            changed = true;
        }
        else if (!next && (settings->swd_draft > 0U))
        {
            --settings->swd_draft;
            changed = true;
        }
    }
    else
    {
        uint8_t candidate = settings->swd_selected;
        for (size_t attempt = 0U; attempt < PROBE_UI_SWD_ENTRIES; ++attempt)
        {
            candidate = next ? (uint8_t)((candidate + 1U) % PROBE_UI_SWD_ENTRIES)
                             : ((candidate == 0U) ? (uint8_t)(PROBE_UI_SWD_ENTRIES - 1U)
                                                  : (uint8_t)(candidate - 1U));
            if (settings_swd_enabled(settings, candidate))
            {
                break;
            }
        }
        changed = (candidate != settings->swd_selected);
        settings->swd_selected = candidate;
    }
    return changed;
}

/** @brief Handle nested SWD editing; a hold cancels a draft before it leaves for Target. */
static bool settings_swd_input(probe_ui_settings_t *settings, probe_ui_settings_action_t action)
{
    bool changed = false;
    if ((action == PROBE_UI_SETTINGS_NEXT) || (action == PROBE_UI_SETTINGS_PREVIOUS))
    {
        changed = settings_swd_move(settings, action == PROBE_UI_SETTINGS_NEXT);
    }
    else if (action == PROBE_UI_SETTINGS_SELECT)
    {
        if (settings->swd_selected == PROBE_UI_SWD_ITEMS)
        {
            settings->page = PROBE_UI_SETTINGS_TARGET;
            settings->choice = settings->values[1];
        }
        else
        {
            if (settings->swd_editing)
            {
                settings->swd_values[settings->swd_selected] = settings->swd_draft;
            }
            else
            {
                settings->swd_draft = settings->swd_values[settings->swd_selected];
            }
            settings->swd_editing = !settings->swd_editing;
        }
        changed = true;
    }
    else if (action == PROBE_UI_SETTINGS_CANCEL)
    {
        if (settings->swd_editing)
        {
            settings->swd_editing = false;
        }
        else
        {
            settings->page = PROBE_UI_SETTINGS_TARGET;
            settings->choice = settings->values[1];
        }
        changed = true;
    }
    else
    {
        /* Unknown actions preserve both the navigation position and committed settings. */
    }
    return changed;
}

/** @brief Require an active, named station before allowing setup to be switched OFF. */
static bool settings_wifi_has_station(const probe_ui_wifi_view_t *view)
{
    return view->station_connected && (view->station_ssid[0] != '\0');
}

/** @brief Navigate setup/Back; an unconfigured or disconnected station keeps setup ON. */
static bool settings_wifi_input(probe_ui_settings_t *settings, probe_ui_settings_action_t action)
{
    bool changed = false;
    if ((action == PROBE_UI_SETTINGS_NEXT) || (action == PROBE_UI_SETTINGS_PREVIOUS))
    {
        settings->wifi_selected = (settings->wifi_selected == 0U) ? 1U : 0U;
        changed = true;
    }
    else if ((action == PROBE_UI_SETTINGS_CANCEL) ||
             ((action == PROBE_UI_SETTINGS_SELECT) && (settings->wifi_selected == 1U)))
    {
        (void)memset(&settings->wifi, 0, sizeof(settings->wifi));
        settings->wifi_setup_requested = false;
        settings->page = PROBE_UI_SETTINGS_LINK;
        settings->choice = settings->values[2];
        changed = true;
    }
    else if ((action == PROBE_UI_SETTINGS_SELECT) && !settings->wifi.busy &&
             settings->wifi.portal_ready && settings_wifi_has_station(&settings->wifi))
    {
        settings->wifi_setup_requested = true;
        changed = true;
    }
    else
    {
        /* Busy/unavailable setup or a missing STA keeps its state. Back remains available. */
    }
    return changed;
}

/** @brief Place a known, at-most-sixteen-character label in the center of a cleared row. */
static void settings_center(char row[PROBE_UI_SETTINGS_COLUMNS + 1U], const char *text)
{
    const size_t length = strlen(text);
    const size_t left = (PROBE_UI_SETTINGS_COLUMNS - length) / 2U;
    (void)memcpy(&row[left], text, length);
}

/** @brief Draw the common final-row Back action on every child page. */
static void settings_back_row(char row[PROBE_UI_SETTINGS_COLUMNS + 1U], bool selected)
{
    row[0] = selected ? '>' : ' ';
    (void)memcpy(&row[1], "Back", 4U);
    row[15] = '<';
}

/** @brief Compose SWD details with a title gap, bracketed draft and nRST-dependent row lock. */
static void settings_swd_render(const probe_ui_settings_t *settings,
                                char lines[PROBE_UI_SETTINGS_ROWS][PROBE_UI_SETTINGS_COLUMNS + 1U])
{
    settings_center(lines[0], "SWD Settings");
    for (size_t field = 0U; field < PROBE_UI_SWD_ITEMS; ++field)
    {
        char *const row = lines[field + 2U];
        const bool enabled = settings_swd_enabled(settings, field);
        const bool selected = enabled && (field == settings->swd_selected);
        const bool editing = selected && settings->swd_editing;
        const uint8_t choice = editing ? settings->swd_draft : settings->swd_values[field];
        const char *const value = enabled ? swd_items[field].choices[choice] : "--";
        row[0] = selected ? '>' : ' ';
        (void)memcpy(&row[1], swd_items[field].label, strlen(swd_items[field].label));
        row[10] = editing ? '[' : ' ';
        (void)memcpy(&row[15U - strlen(value)], value, strlen(value));
        row[15] = editing ? ']' : ' ';
    }
    settings_center(lines[6], (settings->swd_selected == PROBE_UI_SWD_ITEMS)
                                  ? "SW: back"
                                  : (settings->swd_editing ? "SW: save" : "SW: edit"));
    settings_back_row(lines[7], settings->swd_selected == PROBE_UI_SWD_ITEMS);
}

/** @brief Reset caller-owned state; no service or persistent setting is touched. */
void probe_ui_settings_init(probe_ui_settings_t *settings)
{
    (void)memset(settings, 0, sizeof(*settings));
    for (size_t item = 0U; item < PROBE_UI_SETTINGS_VALUES; ++item)
    {
        settings->values[item] = items[item].initial;
    }
    for (size_t field = 0U; field < PROBE_UI_SWD_ITEMS; ++field)
    {
        settings->swd_values[field] = swd_items[field].initial;
    }
}

/** @brief Apply a navigation/selection action without touching hardware or running a flash job. */
bool probe_ui_settings_input(probe_ui_settings_t *settings, probe_ui_settings_action_t action)
{
    if (settings->page == PROBE_UI_SETTINGS_WIFI)
    {
        return settings_wifi_input(settings, action);
    }
    if (settings->page == PROBE_UI_SETTINGS_SWD)
    {
        return settings_swd_input(settings, action);
    }
    bool changed = false;
    if ((action == PROBE_UI_SETTINGS_PREVIOUS) || (action == PROBE_UI_SETTINGS_NEXT))
    {
        const bool next = (action == PROBE_UI_SETTINGS_NEXT);
        if (settings->page != PROBE_UI_SETTINGS_INFO)
        {
            changed = (settings->page == PROBE_UI_SETTINGS_HOME)
                          ? settings_move_home(settings, next)
                          : settings_move_choice(settings, next);
        }
    }
    else if (action == PROBE_UI_SETTINGS_SELECT)
    {
        if (settings->page == PROBE_UI_SETTINGS_HOME)
        {
            if (settings_enabled(settings, settings->selected))
            {
                settings->page = (probe_ui_settings_page_t)(settings->selected + 1U);
                if (settings->selected < PROBE_UI_SETTINGS_VALUES)
                {
                    settings->choice = settings->values[settings->selected];
                }
                changed = true;
            }
        }
        else if (settings->page == PROBE_UI_SETTINGS_INFO)
        {
            settings->page = PROBE_UI_SETTINGS_HOME;
            changed = true;
        }
        else
        {
            const size_t item = (size_t)settings->page - 1U;
            if (settings->choice == items[item].count)
            {
                settings->page = PROBE_UI_SETTINGS_HOME;
                settings->choice = settings->values[item];
            }
            else
            {
                settings->values[item] = settings->choice;
                settings->selected = (uint8_t)item;
                if ((item == 1U) && (settings->choice == 0U))
                {
                    settings->page = PROBE_UI_SETTINGS_SWD;
                    settings->swd_selected = 0U;
                    settings->swd_editing = false;
                }
                else if ((item == 2U) && (settings->choice == 0U))
                {
                    settings->page = PROBE_UI_SETTINGS_WIFI;
                    settings->wifi_selected = 0U;
                    settings->wifi_setup_requested = false;
                }
                else
                {
                    settings->page = PROBE_UI_SETTINGS_HOME;
                }
            }
            changed = true;
        }
    }
    else if ((action == PROBE_UI_SETTINGS_CANCEL) && (settings->page != PROBE_UI_SETTINGS_HOME))
    {
        settings->page = PROBE_UI_SETTINGS_HOME;
        if (settings->selected < PROBE_UI_SETTINGS_VALUES)
        {
            settings->choice = settings->values[settings->selected];
        }
        changed = true;
    }
    else
    {
        /* A hold on home and unrecognized actions leave the current page unchanged. */
    }
    return changed;
}

/** @brief Render home, a choice list or read-only credits into a fixed-size character grid. */
void probe_ui_settings_render(const probe_ui_settings_t *settings,
                              char lines[PROBE_UI_SETTINGS_ROWS][PROBE_UI_SETTINGS_COLUMNS + 1U])
{
    for (size_t row = 0U; row < PROBE_UI_SETTINGS_ROWS; ++row)
    {
        (void)memset(lines[row], ' ', PROBE_UI_SETTINGS_COLUMNS);
        lines[row][PROBE_UI_SETTINGS_COLUMNS] = '\0';
    }
    if (settings->page == PROBE_UI_SETTINGS_HOME)
    {
        settings_center(lines[0], "Settings");
        for (size_t item = 0U; item < PROBE_UI_SETTINGS_VALUES; ++item)
        {
            char *const row = lines[item + 2U];
            const bool enabled = settings_enabled(settings, item);
            const char *const value = enabled ? items[item].choices[settings->values[item]] : "--";
            row[0] = (enabled && (item == settings->selected)) ? '>' : ' ';
            (void)memcpy(&row[1], items[item].label, strlen(items[item].label));
            (void)memcpy(&row[15U - strlen(value)], value, strlen(value));
            row[15] = enabled ? '>' : ' ';
        }
        (void)memcpy(&lines[7][1], "Info", 4U);
        lines[7][0] = (settings->selected == 4U) ? '>' : ' ';
        lines[7][15] = '>';
    }
    else if (settings->page == PROBE_UI_SETTINGS_INFO)
    {
        settings_center(lines[0], "Info");
        settings_center(lines[2], "MAGIC DEBUGGER");
        settings_center(lines[3], "TOOL");
        settings_center(lines[4], "(C)2026 GPL-3.0+");
        settings_center(lines[5], "Nguyen Hoang");
        settings_center(lines[6], "Minh Triet");
        settings_back_row(lines[7], true);
    }
    else if (settings->page == PROBE_UI_SETTINGS_SWD)
    {
        settings_swd_render(settings, lines);
    }
    else if (settings->page == PROBE_UI_SETTINGS_WIFI)
    {
        settings_center(lines[0], "WiFi config");
        (void)memcpy(lines[2], "STA:", 4U);
        const char *station =
            settings_wifi_has_station(&settings->wifi) ? settings->wifi.station_ssid : " NONE";
        const size_t station_length = strlen(station);
        (void)memcpy(&lines[2][4], station, (station_length > 12U) ? 12U : station_length);
        lines[3][0] = (settings->wifi_selected == 0U) ? '>' : ' ';
        (void)memcpy(&lines[3][1], "STA setup", 9U);
        const char *setup = !settings->wifi.portal_ready   ? "N/A"
                            : settings->wifi.setup_enabled ? "ON"
                                                           : "OFF";
        (void)memcpy(&lines[3][16U - strlen(setup)], setup, strlen(setup));
        if (settings->wifi.setup_enabled && settings->wifi.portal_ready)
        {
            (void)memcpy(&lines[4][1], "IP:192.168.4.1", 14U);
            (void)memcpy(&lines[5][1], "AP:", 3U);
            const size_t ssid_length = strlen(settings->wifi.ap_ssid);
            (void)memcpy(&lines[5][4], settings->wifi.ap_ssid,
                         (ssid_length > 12U) ? 12U : ssid_length);
            (void)memcpy(&lines[6][1], "PW:", 3U);
            const size_t password_length = strlen(settings->wifi.ap_password);
            (void)memcpy(&lines[6][4], settings->wifi.ap_password,
                         (password_length > 12U) ? 12U : password_length);
        }
        settings_back_row(lines[7], settings->wifi_selected == 1U);
    }
    else
    {
        const size_t item = (size_t)settings->page - 1U;
        settings_center(lines[0], items[item].title);
        for (size_t choice = 0U; choice < items[item].count; ++choice)
        {
            char *const row = lines[choice + 2U];
            const char *const value = items[item].choices[choice];
            const size_t length = strlen(value);
            row[0] = (choice == settings->choice) ? '>' : ' ';
            (void)memcpy(&row[2], value, length);
            if (settings->page == PROBE_UI_SETTINGS_OLED)
            {
                row[2U + length] = '%';
            }
            row[15] = (choice == settings->values[item]) ? '*' : ' ';
        }
        const bool back_selected = (settings->choice == items[item].count);
        if (items[item].count < 5U)
        {
            settings_center(lines[6], back_selected ? "SW: back" : "SW: select");
        }
        settings_back_row(lines[7], back_selected);
    }
}

/** @brief Return a live OLED-page preview or the saved-in-RAM brightness elsewhere. */
uint8_t probe_ui_settings_oled_percent(const probe_ui_settings_t *settings)
{
    const uint8_t choice =
        ((settings->page == PROBE_UI_SETTINGS_OLED) && (settings->choice < items[3].count))
            ? settings->choice
            : settings->values[3];
    return oled_percentages[choice];
}

/** @brief Translate committed SWD choices to nominal Hz, milliseconds and explicit pin flags. */
void probe_ui_settings_swd_get(const probe_ui_settings_t *settings, probe_ui_swd_config_t *config)
{
    config->frequency_hz = swd_frequencies_hz[settings->swd_values[SWD_FREQUENCY]];
    config->reset_pulse_ms = swd_reset_pulses_ms[settings->swd_values[SWD_RESET_PULSE]];
    config->use_nrst = (settings->swd_values[SWD_NRST] != 0U);
    config->use_swo = (settings->swd_values[SWD_SWO] != 0U);
}

/** @brief Copy a live page snapshot only while the user has explicitly opened Wi-Fi setup. */
bool probe_ui_settings_wifi_update(probe_ui_settings_t *settings, const probe_ui_wifi_view_t *view)
{
    bool changed = false;
    if (settings->page == PROBE_UI_SETTINGS_WIFI)
    {
        probe_ui_wifi_view_t current = *view;
        if (!settings_wifi_has_station(&current))
        {
            current.setup_enabled = true;
        }
        changed = (strcmp(settings->wifi.station_ssid, current.station_ssid) != 0) ||
                  (strcmp(settings->wifi.ap_ssid, current.ap_ssid) != 0) ||
                  (strcmp(settings->wifi.ap_password, current.ap_password) != 0) ||
                  (settings->wifi.portal_ready != current.portal_ready) ||
                  (settings->wifi.setup_enabled != current.setup_enabled) ||
                  (settings->wifi.station_connected != current.station_connected) ||
                  (settings->wifi.busy != current.busy);
        settings->wifi = current;
    }
    return changed;
}

/** @brief Hand off one web-mode toggle; the live snapshot remains authoritative. */
bool probe_ui_settings_wifi_setup_take(probe_ui_settings_t *settings, bool *enabled)
{
    const bool requested = settings->wifi_setup_requested;
    if (requested)
    {
        *enabled = !settings->wifi.setup_enabled;
        settings->wifi_setup_requested = false;
    }
    return requested;
}
