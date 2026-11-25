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

/** @brief Move a child-page draft; OLED clamps while mode-selection lists wrap. */
static bool settings_move_choice(probe_ui_settings_t *settings, bool next)
{
    const size_t item = (size_t)settings->page - 1U;
    const uint8_t count = items[item].count;
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

/** @brief Place a known, at-most-sixteen-character label in the center of a cleared row. */
static void settings_center(char row[PROBE_UI_SETTINGS_COLUMNS + 1U], const char *text)
{
    const size_t length = strlen(text);
    const size_t left = (PROBE_UI_SETTINGS_COLUMNS - length) / 2U;
    (void)memcpy(&row[left], text, length);
}

/** @brief Reset caller-owned state; no service or persistent setting is touched. */
void probe_ui_settings_init(probe_ui_settings_t *settings)
{
    (void)memset(settings, 0, sizeof(*settings));
    for (size_t item = 0U; item < PROBE_UI_SETTINGS_VALUES; ++item)
    {
        settings->values[item] = items[item].initial;
    }
}

/** @brief Apply a navigation/selection action without touching hardware or running a flash job. */
bool probe_ui_settings_input(probe_ui_settings_t *settings, probe_ui_settings_action_t action)
{
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
            settings->values[item] = settings->choice;
            settings->page = PROBE_UI_SETTINGS_HOME;
            settings->selected = (uint8_t)item;
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
        settings_center(lines[4], "(C) 2026");
        settings_center(lines[5], "Nguyen Hoang");
        settings_center(lines[6], "Minh Triet");
        settings_center(lines[7], "GPL-3.0-or-later");
    }
    else
    {
        const size_t item = (size_t)settings->page - 1U;
        settings_center(lines[0], items[item].title);
        for (size_t choice = 0U; choice < items[item].count; ++choice)
        {
            char *const row = lines[choice + 1U];
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
        settings_center(lines[6], "SW: select");
        settings_center(lines[7], "Hold SW: back");
    }
}

/** @brief Return a live OLED-page preview or the saved-in-RAM brightness elsewhere. */
uint8_t probe_ui_settings_oled_percent(const probe_ui_settings_t *settings)
{
    const uint8_t choice =
        (settings->page == PROBE_UI_SETTINGS_OLED) ? settings->choice : settings->values[3];
    return oled_percentages[choice];
}
