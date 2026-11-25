/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_ui_settings.c
 * @brief Provisional settings choices, encoder transitions and fixed-cell text layout.
 */
#include "probe_ui_settings.h"
#include <stddef.h>
#include <string.h>

/** @brief Bounded sample parameter; labels use nine cells and values use at most four. */
typedef struct
{
    char label[10];
    char choices[6][5];
    uint8_t count;
    uint8_t initial;
} settings_item_t;

/* Replace these UI examples when the actual tool parameters and service contracts are agreed. */
static const settings_item_t items[PROBE_UI_SETTINGS_ITEMS] = {
    {"SWD kHz", {"100", "400", "1000", "2000", "4000", "8000"}, 6U, 1U},
    {"Reset ms", {"10", "20", "50", "100", "200", "500"}, 6U, 3U},
    {"Retry", {"1", "2", "3", "5", "8"}, 5U, 2U},
    {"Auto halt", {"OFF", "ON"}, 2U, 1U},
    {"SWO kHz", {"OFF", "250", "500", "1000", "2000", "4000"}, 6U, 0U},
    {"Log level", {"ERR", "WARN", "INFO", "DBG"}, 4U, 2U},
    {"OLED %", {"10", "25", "50", "75", "100"}, 5U, 2U}};

/** @brief Reset caller-owned state without accessing any hardware configuration. */
void probe_ui_settings_init(probe_ui_settings_t *settings)
{
    (void)memset(settings, 0, sizeof(*settings));
    for (size_t index = 0U; index < PROBE_UI_SETTINGS_ITEMS; ++index)
    {
        settings->values[index] = items[index].initial;
    }
}

/** @brief Apply one logical encoder action and report whether the displayed state changed. */
bool probe_ui_settings_input(probe_ui_settings_t *settings, probe_ui_settings_action_t action)
{
    bool changed = false;
    if ((action == PROBE_UI_SETTINGS_PREVIOUS) || (action == PROBE_UI_SETTINGS_NEXT))
    {
        const bool next = (action == PROBE_UI_SETTINGS_NEXT);
        if (settings->editing)
        {
            if (next && ((settings->draft + 1U) < items[settings->selected].count))
            {
                ++settings->draft;
                changed = true;
            }
            else if (!next && (settings->draft > 0U))
            {
                --settings->draft;
                changed = true;
            }
        }
        else
        {
            if (next)
            {
                settings->selected = (uint8_t)((settings->selected + 1U) % PROBE_UI_SETTINGS_ITEMS);
            }
            else
            {
                settings->selected = (settings->selected == 0U)
                                         ? (uint8_t)(PROBE_UI_SETTINGS_ITEMS - 1U)
                                         : (uint8_t)(settings->selected - 1U);
            }
            changed = true;
        }
    }
    else if (action == PROBE_UI_SETTINGS_SELECT)
    {
        if (settings->editing)
        {
            settings->values[settings->selected] = settings->draft;
        }
        else
        {
            settings->draft = settings->values[settings->selected];
        }
        settings->editing = !settings->editing;
        changed = true;
    }
    else if ((action == PROBE_UI_SETTINGS_CANCEL) && settings->editing)
    {
        settings->editing = false;
        changed = true;
    }
    else
    {
        /* A hold outside editing and unrecognized actions leave the settings screen unchanged. */
    }
    return changed;
}

/** @brief Render the header, seven sample parameters and a bracketed draft when editing. */
void probe_ui_settings_render(const probe_ui_settings_t *settings,
                              char lines[PROBE_UI_SETTINGS_ROWS][PROBE_UI_SETTINGS_COLUMNS + 1U])
{
    for (size_t row = 0U; row < PROBE_UI_SETTINGS_ROWS; ++row)
    {
        (void)memset(lines[row], ' ', PROBE_UI_SETTINGS_COLUMNS);
        lines[row][PROBE_UI_SETTINGS_COLUMNS] = '\0';
    }
    (void)memcpy(lines[0], "    Settings    ", PROBE_UI_SETTINGS_COLUMNS);
    for (size_t index = 0U; index < PROBE_UI_SETTINGS_ITEMS; ++index)
    {
        char *const row = lines[index + 1U];
        const bool selected = (index == settings->selected);
        const bool editing = selected && settings->editing;
        const uint8_t choice = editing ? settings->draft : settings->values[index];
        const char *const value = items[index].choices[choice];
        const size_t value_length = strlen(value);
        row[0] = selected ? '>' : ' ';
        (void)memcpy(&row[1], items[index].label, strlen(items[index].label));
        row[10] = editing ? '[' : ' ';
        (void)memcpy(&row[15U - value_length], value, value_length);
        row[15] = editing ? ']' : ' ';
    }
}
