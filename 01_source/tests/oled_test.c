/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/**
 * @file oled_test.c
 * @brief SH1106 RAM-model and OLED lifecycle regression tests using the production driver.
 *
 * The controller model has 132 columns per page and never advances to a new page implicitly.
 * Unsupported SSD1306 commands abort the test. No real I2C bus or OLED is accessed.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "probe_oled.h"
#include "probe_ui_settings.h"
#include "sdkconfig.h"
#include "esp_lcd_panel_io.h"
#include "freertos/task.h"

struct oled_test_bus
{
    bool owned;
};
struct oled_test_io
{
    bool owned;
};
static struct oled_test_bus bus_token;
static struct oled_test_io io_token;
static uint8_t ram[8][132];
static uint8_t selected_page;
static unsigned int selected_column;
static uint8_t pending_argument;
static uint8_t contrast;
static bool dc_dc_enabled;
static bool display_on;
static bool first_display_enable = true;
static bool segment_remapped = true;
static bool com_reversed = true;
static uint16_t ack_address = 0x3cU;
static unsigned int probes;
static unsigned int io_calls;
static unsigned int data_calls;
static unsigned int data_bytes;
static unsigned int fail_io_call;
static bool fail_bus;
static bool fail_io_create;
static TickType_t delayed_ticks;

/** @brief Require bounded rows, one visible cursor and no home cursor on a locked item. */
static void check_settings_layout(const probe_ui_settings_t *settings)
{
    struct
    {
        uint8_t before[8];
        char rows[8][17];
        uint8_t after[8];
    } guarded;
    (void)memset(&guarded, 0xa5, sizeof(guarded));
    probe_ui_settings_render(settings, guarded.rows);
    unsigned int cursors = 0U;
    for (size_t row = 0U; row < 8U; ++row)
    {
        assert(guarded.before[row] == 0xa5U && guarded.after[row] == 0xa5U);
        assert(strlen(guarded.rows[row]) == 16U);
        assert(strstr(guarded.rows[row], "Next") == NULL);
        assert(strstr(guarded.rows[row], "Mode preview") == NULL);
        assert(strstr(guarded.rows[row], "Hold SW") == NULL);
        if (settings->page == PROBE_UI_SETTINGS_HOME)
        {
            assert(strstr(guarded.rows[row], "Back") == NULL);
        }
        if (guarded.rows[row][0] == '>')
        {
            ++cursors;
        }
    }
    assert(cursors == 1U);
    assert(strcmp(guarded.rows[1], "                ") == 0);
    assert(guarded.rows[0][0] != '>' && settings->selected < 5U);
    if (settings->page == PROBE_UI_SETTINGS_HOME)
    {
        assert(strcmp(guarded.rows[0], "    Settings    ") == 0);
        assert(strcmp(guarded.rows[1], "                ") == 0);
        assert(strcmp(guarded.rows[6], "                ") == 0);
        for (size_t item = 0U; item < 4U; ++item)
        {
            const bool enabled = (settings->values[0] == 0U) || (item == 0U) || (item == 3U);
            assert(guarded.rows[item + 2U][0] == ((item == settings->selected) ? '>' : ' '));
            assert(guarded.rows[item + 2U][15] == (enabled ? '>' : ' '));
            if (!enabled)
            {
                assert(strstr(guarded.rows[item + 2U], "--") != NULL);
                assert(item != settings->selected);
            }
        }
        assert(guarded.rows[7][0] == ((settings->selected == 4U) ? '>' : ' '));
        assert(strstr(guarded.rows[7], "Info") != NULL && guarded.rows[7][15] == '>');
    }
    else if (settings->page == PROBE_UI_SETTINGS_INFO)
    {
        assert(strcmp(guarded.rows[0], "      Info      ") == 0);
        assert(strcmp(guarded.rows[2], " MAGIC DEBUGGER ") == 0);
        assert(strcmp(guarded.rows[3], "      TOOL      ") == 0);
        assert(strcmp(guarded.rows[4], "(C)2026 GPL-3.0+") == 0);
        assert(strcmp(guarded.rows[5], "  Nguyen Hoang  ") == 0);
        assert(strcmp(guarded.rows[6], "   Minh Triet   ") == 0);
        assert(strcmp(guarded.rows[7], ">Back          <") == 0);
    }
    else if (settings->page == PROBE_UI_SETTINGS_SWD)
    {
        assert(strcmp(guarded.rows[0], "  SWD Settings  ") == 0);
        assert(strcmp(guarded.rows[1], "                ") == 0);
        assert(settings->swd_selected < 5U);
        assert(guarded.rows[7][0] == ((settings->swd_selected == 4U) ? '>' : ' '));
        assert(strstr(guarded.rows[7], "Back") != NULL && guarded.rows[7][15] == '<');
        for (size_t field = 0U; field < 4U; ++field)
        {
            const bool enabled = (field != 2U) || (settings->swd_values[1] != 0U);
            const bool selected = (field == settings->swd_selected);
            const bool editing = selected && settings->swd_editing;
            assert(guarded.rows[field + 2U][0] == (selected ? '>' : ' '));
            assert(guarded.rows[field + 2U][10] == (editing ? '[' : ' '));
            assert(guarded.rows[field + 2U][15] == (editing ? ']' : ' '));
            if (!enabled)
            {
                assert(!selected && strstr(guarded.rows[field + 2U], "--") != NULL);
            }
        }
    }
    else if (settings->page == PROBE_UI_SETTINGS_WIFI)
    {
        assert(settings->wifi_selected < 2U);
        assert(strncmp(guarded.rows[2], "STA:", 4U) == 0);
        assert(guarded.rows[3][0] == ((settings->wifi_selected == 0U) ? '>' : ' '));
        assert(guarded.rows[7][0] == ((settings->wifi_selected == 1U) ? '>' : ' '));
        assert((strstr(guarded.rows[4], "192.168.4.1") != NULL) ==
               (settings->wifi.setup_enabled && settings->wifi.portal_ready));
        for (size_t row = 0U; row < 8U; ++row)
        {
            assert(strstr(guarded.rows[row], "Ping") == NULL);
        }
        if (!settings->wifi.setup_enabled || !settings->wifi.portal_ready)
        {
            for (size_t row = 4U; row <= 6U; ++row)
            {
                assert(strcmp(guarded.rows[row], "                ") == 0);
            }
        }
        assert(strstr(guarded.rows[7], "Back") != NULL);
    }
    else
    {
        const uint8_t counts[] = {2U, 4U, 3U, 5U};
        const size_t item = (size_t)settings->page - 1U;
        assert(item < 4U && settings->choice <= counts[item]);
        assert(guarded.rows[7][0] == ((settings->choice == counts[item]) ? '>' : ' '));
        assert(strstr(guarded.rows[7], "Back") != NULL && guarded.rows[7][15] == '<');
        for (size_t choice = 0U; choice < counts[item]; ++choice)
        {
            assert(guarded.rows[choice + 2U][0] == ((choice == settings->choice) ? '>' : ' '));
            assert(guarded.rows[choice + 2U][15] ==
                   ((choice == settings->values[item]) ? '*' : ' '));
        }
    }
}

/** @brief Verify five home entries, title spacing, read-only Info and isolated child drafts. */
static void test_settings_navigation(void)
{
    probe_ui_settings_t settings;
    probe_ui_settings_init(&settings);
    check_settings_layout(&settings);
    char rows[8][17];
    probe_ui_settings_render(&settings, rows);
    assert(strcmp(rows[2], ">Flash   REMOTE>") == 0);
    assert(strcmp(rows[3], " Target     SWD>") == 0);
    assert(strcmp(rows[4], " Link      WiFi>") == 0);
    assert(strcmp(rows[5], " OLED %      50>") == 0);
    assert(strcmp(rows[7], " Info          >") == 0);
    assert(probe_ui_settings_oled_percent(&settings) == 50U);
    assert(!probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_CANCEL));
    assert(!probe_ui_settings_input(&settings, (probe_ui_settings_action_t)99));
    for (unsigned int step = 0U; step < 10U; ++step)
    {
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        assert(settings.selected == (step + 1U) % 5U);
        check_settings_layout(&settings);
    }
    for (unsigned int step = 0U; step < 10U; ++step)
    {
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
        assert(settings.selected == 4U - step % 5U);
        check_settings_layout(&settings);
    }
    for (uint8_t item = 0U; item < 5U; ++item)
    {
        assert(settings.selected == item && settings.page == PROBE_UI_SETTINGS_HOME);
        uint8_t committed[4];
        (void)memcpy(committed, settings.values, sizeof(committed));
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
        assert(settings.page == (probe_ui_settings_page_t)(item + 1U));
        check_settings_layout(&settings);
        if (item == 4U)
        {
            assert(!probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
            assert(!probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
            assert(settings.page == PROBE_UI_SETTINGS_HOME && settings.selected == 4U);
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
        }
        else
        {
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        }
        assert(memcmp(settings.values, committed, sizeof(committed)) == 0);
        check_settings_layout(&settings);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_CANCEL));
        assert(settings.page == PROBE_UI_SETTINGS_HOME && settings.selected == item);
        assert(memcmp(settings.values, committed, sizeof(committed)) == 0);
        assert(probe_ui_settings_oled_percent(&settings) == 50U);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    }
}

/** @brief Exercise every child choice, cyclic mode lists, OLED limits and contrast cancellation. */
static void test_settings_choices(void)
{
    const uint8_t counts[] = {2U, 4U, 3U, 5U};
    const uint8_t percentages[] = {10U, 25U, 50U, 75U, 100U};
    for (uint8_t item = 0U; item < 4U; ++item)
    {
        probe_ui_settings_t settings;
        probe_ui_settings_init(&settings);
        for (uint8_t step = 0U; step < item; ++step)
        {
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        }
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
        for (unsigned int step = 0U; step < 10U; ++step)
        {
            (void)probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS);
            check_settings_layout(&settings);
        }
        if (item == 3U)
        {
            assert(settings.choice == 0U);
            assert(!probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
            assert(probe_ui_settings_oled_percent(&settings) == 10U);
            assert(settings.values[3] == 2U);
        }
        else
        {
            for (uint8_t step = 0U; (step < counts[item]) && (settings.choice != 0U); ++step)
            {
                assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
            }
            assert(settings.choice == 0U);
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
            assert(settings.choice == counts[item]);
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
            assert(settings.choice == 0U);
        }
        for (uint8_t choice = 0U; choice < counts[item]; ++choice)
        {
            assert(settings.choice == choice);
            if (item == 3U)
            {
                assert(probe_ui_settings_oled_percent(&settings) == percentages[choice]);
            }
            check_settings_layout(&settings);
            if (choice + 1U < counts[item])
            {
                assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
            }
        }
        if (item == 3U)
        {
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
            assert(settings.choice == counts[item]);
            assert(probe_ui_settings_oled_percent(&settings) == 50U);
            assert(!probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
        }
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
        assert(settings.page == PROBE_UI_SETTINGS_HOME);
        assert(settings.values[item] == counts[item] - 1U);
        check_settings_layout(&settings);
    }
}

/** @brief Every child Back returns to its parent without committing a draft or leaking its index.
 */
static void test_child_back_actions(void)
{
    const uint8_t counts[] = {2U, 4U, 3U, 5U};
    for (uint8_t item = 0U; item < 5U; ++item)
    {
        probe_ui_settings_t settings;
        probe_ui_settings_init(&settings);
        for (uint8_t step = 0U; step < item; ++step)
        {
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        }
        uint8_t committed[4];
        (void)memcpy(committed, settings.values, sizeof(committed));
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
        if (item < 4U)
        {
            for (unsigned int step = 0U; (step < 6U) && (settings.choice != counts[item]); ++step)
            {
                assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
            }
            assert(settings.choice == counts[item]);
            assert(probe_ui_settings_oled_percent(&settings) == 50U);
        }
        check_settings_layout(&settings);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
        assert(settings.page == PROBE_UI_SETTINGS_HOME && settings.selected == item);
        assert(memcmp(settings.values, committed, sizeof(committed)) == 0);
        assert(probe_ui_settings_oled_percent(&settings) == 50U);
        check_settings_layout(&settings);
    }
    (void)puts("PASS: Back on Flash/Target/Link/OLED/Info, draft discard and OLED restoration");
}

/** @brief Lock Target/Link in SDCARD while preserving OLED adjustment and read-only Info access. */
static void test_settings_sd_lock(void)
{
    probe_ui_settings_t settings;
    probe_ui_settings_init(&settings);
    for (uint8_t item = 1U; item < 4U; ++item)
    {
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        assert(settings.selected == item);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    }
    const uint8_t target = settings.values[1];
    const uint8_t link = settings.values[2];
    const uint8_t oled = settings.values[3];
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(settings.selected == 4U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(settings.selected == 0U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings.values[0] == 1U && settings.selected == 0U);
    check_settings_layout(&settings);
    for (unsigned int step = 0U; step < 4U; ++step)
    {
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        assert(settings.selected == 3U);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
        assert(settings.page == PROBE_UI_SETTINGS_OLED);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
        assert(probe_ui_settings_oled_percent(&settings) == 50U);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_CANCEL));
        assert(settings.selected == 3U && settings.values[3] == oled);
        assert(probe_ui_settings_oled_percent(&settings) == 75U);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        assert(settings.selected == 4U);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
        assert(settings.page == PROBE_UI_SETTINGS_INFO);
        check_settings_layout(&settings);
        assert(!probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_CANCEL));
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        assert(settings.selected == 0U);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
        assert(settings.selected == 4U);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
        assert(settings.selected == 3U);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
        assert(settings.selected == 0U);
        check_settings_layout(&settings);
    }
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings.selected == 3U && settings.values[3] == 2U);
    assert(probe_ui_settings_oled_percent(&settings) == 50U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(settings.selected == 0U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings.page == PROBE_UI_SETTINGS_FLASH && settings.choice == 1U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_CANCEL));
    assert(settings.values[0] == 1U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings.values[0] == 0U);
    assert(settings.values[1] == target && settings.values[2] == link && settings.values[3] == 2U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(settings.selected == 1U);
    check_settings_layout(&settings);
}

/** @brief Enter SWD from the default home, using only public input actions. */
static void open_swd_settings(probe_ui_settings_t *settings)
{
    probe_ui_settings_init(settings);
    assert(probe_ui_settings_input(settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_input(settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings->page == PROBE_UI_SETTINGS_TARGET && settings->choice == 0U);
    assert(probe_ui_settings_input(settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings->page == PROBE_UI_SETTINGS_SWD && settings->swd_selected == 0U);
}

/** @brief Require the committed SWD output in Hz/ms; uncommitted drafts must not leak out. */
static void check_swd_config(const probe_ui_settings_t *settings, uint32_t hz, bool nrst,
                             uint16_t pulse_ms, bool swo)
{
    probe_ui_swd_config_t config;
    probe_ui_settings_swd_get(settings, &config);
    assert(config.frequency_hz == hz && config.use_nrst == nrst);
    assert(config.reset_pulse_ms == pulse_ms && config.use_swo == swo);
    check_settings_layout(settings);
}

/** @brief Verify save/cancel, reset-width row locks, SWO intent and nested Target navigation. */
static void test_swd_settings(void)
{
    probe_ui_settings_t settings;
    open_swd_settings(&settings);
    check_swd_config(&settings, 400000U, true, 100U, false);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
    assert(settings.swd_selected == 4U);
    check_settings_layout(&settings);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings.page == PROBE_UI_SETTINGS_TARGET);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(!probe_ui_settings_input(&settings, (probe_ui_settings_action_t)99));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings.swd_editing);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    check_swd_config(&settings, 400000U, true, 100U, false);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_CANCEL));
    assert(settings.page == PROBE_UI_SETTINGS_SWD && !settings.swd_editing);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    check_swd_config(&settings, 1000000U, true, 100U, false);

    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(settings.swd_selected == 1U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    check_swd_config(&settings, 1000000U, false, 100U, false);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(settings.swd_selected == 3U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
    assert(settings.swd_selected == 1U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(settings.swd_selected == 2U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    check_swd_config(&settings, 1000000U, true, 200U, false);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(settings.swd_selected == 3U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    check_swd_config(&settings, 1000000U, true, 200U, true);

    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(settings.swd_selected == 4U);
    check_settings_layout(&settings);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings.page == PROBE_UI_SETTINGS_TARGET && settings.choice == 0U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings.page == PROBE_UI_SETTINGS_HOME && settings.values[1] == 1U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings.page == PROBE_UI_SETTINGS_SWD && !settings.swd_editing);
    check_swd_config(&settings, 1000000U, true, 200U, true);
    assert(probe_ui_settings_oled_percent(&settings) == 50U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_CANCEL));
    assert(settings.page == PROBE_UI_SETTINGS_TARGET);
}

/** @brief Exercise every frequency/pulse preset and clamp both bounds without wrapping. */
static void test_swd_bounds(void)
{
    const uint32_t frequencies[] = {100000U, 400000U, 1000000U, 2000000U, 4000000U, 8000000U};
    const uint16_t pulses[] = {1U, 5U, 10U, 20U, 50U, 100U, 200U, 500U};
    probe_ui_settings_t settings;
    open_swd_settings(&settings);
    for (unsigned int field = 0U; field <= 2U; field += 2U)
    {
        assert(settings.swd_selected == field);
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
        for (unsigned int step = 0U; step < 8U; ++step)
        {
            (void)probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS);
        }
        assert(!probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
        const uint8_t count = (field == 0U) ? 6U : 8U;
        for (uint8_t choice = 0U; choice < count; ++choice)
        {
            assert(settings.swd_draft == choice && settings.swd_selected == field);
            check_settings_layout(&settings);
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
            probe_ui_swd_config_t config;
            probe_ui_settings_swd_get(&settings, &config);
            assert(config.frequency_hz == ((field == 0U) ? frequencies[choice] : 8000000U));
            assert(config.reset_pulse_ms == ((field == 2U) ? pulses[choice] : 100U));
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
            if ((choice + 1U) < count)
            {
                assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
            }
        }
        assert(!probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_CANCEL));
        assert(!settings.swd_editing && settings.page == PROBE_UI_SETTINGS_SWD);
        if (field == 0U)
        {
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        }
    }
    check_swd_config(&settings, 8000000U, true, 500U, false);
    (void)puts("PASS: nested SWD edits, Hz/ms presets, NRST lock, SWO flag and draft cancellation");
}

/** @brief Open the Wi-Fi configuration page through the real home and Link input flow. */
static void open_wifi_settings(probe_ui_settings_t *settings)
{
    probe_ui_settings_init(settings);
    assert(probe_ui_settings_input(settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_input(settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_input(settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings->page == PROBE_UI_SETTINGS_LINK && settings->choice == 0U);
    assert(probe_ui_settings_input(settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings->page == PROBE_UI_SETTINGS_WIFI);
}

/** @brief Verify access hiding/alignment, forced setup without STA, truncation and Back. */
static void test_wifi_settings(void)
{
    probe_ui_settings_t settings;
    open_wifi_settings(&settings);
    probe_ui_wifi_view_t view = {.station_ssid = "LAB-ROUTER",
                                 .ap_ssid = "MAGIC-PROBE",
                                 .ap_password = "1234567890",
                                 .portal_ready = true,
                                 .station_connected = true};
    assert(probe_ui_settings_wifi_update(&settings, &view));
    assert(!probe_ui_settings_wifi_update(&settings, &view));
    check_settings_layout(&settings);
    char rows[8][17];
    probe_ui_settings_render(&settings, rows);
    assert(strcmp(rows[1], "                ") == 0);
    assert(strcmp(rows[2], "STA:LAB-ROUTER  ") == 0);
    assert(strcmp(rows[3], ">STA setup   OFF") == 0);
    for (size_t row = 4U; row <= 6U; ++row)
    {
        assert(strcmp(rows[row], "                ") == 0);
    }
    bool enabled = false;
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_wifi_setup_take(&settings, &enabled) && enabled);
    assert(!probe_ui_settings_wifi_setup_take(&settings, &enabled));
    assert(!settings.wifi.setup_enabled && settings.wifi.station_connected);
    view.setup_enabled = true;
    assert(probe_ui_settings_wifi_update(&settings, &view));
    probe_ui_settings_render(&settings, rows);
    assert(strstr(rows[3], "ON") != NULL);
    assert(strcmp(rows[4], " IP:192.168.4.1 ") == 0);
    assert(strcmp(rows[5], " AP:MAGIC-PROBE ") == 0);
    assert(strcmp(rows[6], " PW:1234567890  ") == 0);
    assert(strcmp(rows[2], "STA:LAB-ROUTER  ") == 0);
    check_settings_layout(&settings);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_wifi_setup_take(&settings, &enabled) && !enabled);
    view.setup_enabled = false;
    assert(probe_ui_settings_wifi_update(&settings, &view));
    view.busy = true;
    assert(probe_ui_settings_wifi_update(&settings, &view));
    assert(!probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(!probe_ui_settings_wifi_setup_take(&settings, &enabled));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    check_settings_layout(&settings);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings.page == PROBE_UI_SETTINGS_LINK && settings.choice == 0U);
    assert(settings.wifi.ap_password[0] == '\0');
    assert(!settings.wifi_setup_requested);
    assert(!probe_ui_settings_wifi_update(&settings, &view));
    assert(settings.wifi.ap_password[0] == '\0');
    check_settings_layout(&settings);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    view.busy = false;
    view.station_connected = false;
    (void)probe_ui_settings_wifi_update(&settings, &view);
    assert(!probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(!probe_ui_settings_wifi_setup_take(&settings, &enabled));
    probe_ui_settings_render(&settings, rows);
    assert(strcmp(rows[2], "STA: NONE       ") == 0);
    assert(strstr(rows[3], "ON") != NULL && strstr(rows[4], "192.168.4.1") != NULL);
    check_settings_layout(&settings);
    view.station_connected = true;
    view.station_ssid[0] = '\0';
    (void)probe_ui_settings_wifi_update(&settings, &view);
    assert(!probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    probe_ui_settings_render(&settings, rows);
    assert(strcmp(rows[2], "STA: NONE       ") == 0);
    assert(strstr(rows[3], "ON") != NULL);
    (void)memcpy(view.station_ssid, "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456", 33U);
    (void)probe_ui_settings_wifi_update(&settings, &view);
    probe_ui_settings_render(&settings, rows);
    assert(strcmp(rows[2], "STA:ABCDEFGHIJKL") == 0);
    check_settings_layout(&settings);
    view.portal_ready = false;
    (void)probe_ui_settings_wifi_update(&settings, &view);
    assert(!probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    probe_ui_settings_render(&settings, rows);
    assert(strstr(rows[3], "N/A") != NULL);
    check_settings_layout(&settings);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
    assert(settings.wifi_selected == 1U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_CANCEL));
    assert(settings.page == PROBE_UI_SETTINGS_LINK);
    (void)puts(
        "PASS: WiFi title gap, aligned hidden access rows, NONE forced ON, long SSID and Back");
}

/** @brief Run the platform-independent navigation, selection and SD-lock regression cases. */
static void test_settings_model(void)
{
    test_settings_navigation();
    test_settings_choices();
    test_child_back_actions();
    test_settings_sd_lock();
    test_swd_settings();
    test_swd_bounds();
    test_wifi_settings();
    (void)puts("PASS: spaced Settings, read-only Info, child drafts and OLED access in SDCARD");
}

/** @brief Interpret documented SH1106 command bytes, preserving multi-byte command state. */
static void controller_command(uint8_t command)
{
    if (pending_argument != 0U)
    {
        if (pending_argument == 0x81U)
        {
            contrast = command;
        }
        else if (pending_argument == 0xadU)
        {
            assert(!display_on && command == 0x8bU);
            dc_dc_enabled = true;
        }
        pending_argument = 0U;
    }
    else if ((command <= 0x0fU) || ((command >= 0x10U) && (command <= 0x1fU)))
    {
        if (command <= 0x0fU)
        {
            selected_column = (selected_column & 0xf0U) | command;
        }
        else
        {
            selected_column = (selected_column & 0x0fU) | ((unsigned int)(command & 0x0fU) << 4U);
        }
    }
    else if ((command >= 0xb0U) && (command <= 0xb7U))
    {
        selected_page = command & 0x07U;
    }
    else if ((command == 0x81U) || (command == 0xa8U) || (command == 0xadU) || (command == 0xd3U) ||
             (command == 0xd5U) || (command == 0xd9U) || (command == 0xdaU) || (command == 0xdbU))
    {
        pending_argument = command;
    }
    else if ((command == 0xa0U) || (command == 0xa1U))
    {
        segment_remapped = (command == 0xa1U);
    }
    else if ((command == 0xc0U) || (command == 0xc8U))
    {
        com_reversed = (command == 0xc8U);
    }
    else if (command == 0xaeU)
    {
        display_on = false;
    }
    else if (command == 0xafU)
    {
        assert(dc_dc_enabled);
        if (first_display_enable)
        {
            assert(data_bytes == (8U * 132U));
            for (size_t page = 0U; page < 8U; ++page)
            {
                for (size_t column = 0U; column < 132U; ++column)
                {
                    assert(ram[page][column] == 0U);
                }
            }
            first_display_enable = false;
        }
        display_on = true;
    }
    else
    {
        /* In particular, 0x20/0x21/0x22 horizontal/window addressing is not accepted. */
        assert((command == 0x40U) || (command == 0xa4U) || (command == 0xa6U));
    }
}

/** @brief Validate exclusive ownership and optionally fail bus allocation. */
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *config, i2c_master_bus_handle_t *bus)
{
    assert(!bus_token.owned && !io_token.owned);
    assert(config->i2c_port == I2C_NUM_0);
    assert(config->sda_io_num == 8 && config->scl_io_num == 9);
    assert(config->flags.enable_internal_pullup);
    if (fail_bus)
    {
        return ESP_FAIL;
    }
    bus_token.owned = true;
    *bus = &bus_token;
    return ESP_OK;
}

/** @brief Require I/O teardown before bus release. */
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus)
{
    assert(bus == &bus_token && bus_token.owned && !io_token.owned);
    bus_token.owned = false;
    return ESP_OK;
}

/** @brief Model discovery at either supported seven-bit address with a finite timeout. */
esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus, uint16_t address, int timeout_ms)
{
    assert(bus == &bus_token && bus_token.owned);
    assert((address == 0x3cU) || (address == 0x3dU));
    assert(timeout_ms > 0 && timeout_ms <= 30);
    ++probes;
    return (address == ack_address) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

/** @brief Verify I2C command/data framing and bounded transfers before I/O acquisition. */
esp_err_t esp_lcd_new_panel_io_i2c(i2c_master_bus_handle_t bus,
                                   const esp_lcd_panel_io_i2c_config_t *config,
                                   esp_lcd_panel_io_handle_t *io)
{
    assert(bus == &bus_token && bus_token.owned && !io_token.owned);
    assert(config->dev_addr == ack_address && config->scl_speed_hz == 100000U);
    assert(config->control_phase_bytes == 1U && config->dc_bit_offset == 6U);
    assert(config->lcd_cmd_bits == 8 && config->lcd_param_bits == 8);
    assert(config->transaction_timeout_ms > 0 && config->transaction_timeout_ms <= 250);
    if (fail_io_create)
    {
        return ESP_FAIL;
    }
    io_token.owned = true;
    *io = &io_token;
    return ESP_OK;
}

/** @brief Release the simulated I/O token while its bus still exists. */
esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t io)
{
    assert(io == &io_token && io_token.owned && bus_token.owned);
    io_token.owned = false;
    return ESP_OK;
}

/** @brief Decode command bytes or inject one finite I2C timeout. */
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t io, int command,
                                    const void *parameters, size_t size)
{
    assert(io == &io_token && io_token.owned);
    ++io_calls;
    if (io_calls == fail_io_call)
    {
        return ESP_ERR_TIMEOUT;
    }
    assert(command >= 0 && command <= 255);
    controller_command((uint8_t)command);
    const uint8_t *bytes = parameters;
    for (size_t index = 0U; index < size; ++index)
    {
        controller_command(bytes[index]);
    }
    return ESP_OK;
}

/** @brief Apply SH1106 page-local column increments without implicit page changes. */
esp_err_t esp_lcd_panel_io_tx_color(esp_lcd_panel_io_handle_t io, int command, const void *data,
                                    size_t size)
{
    assert(io == &io_token && io_token.owned && command == -1 && pending_argument == 0U);
    ++io_calls;
    if (io_calls == fail_io_call)
    {
        return ESP_ERR_TIMEOUT;
    }
    assert((size == 128U) || (size == 132U));
    assert(selected_column + size <= 132U);
    (void)memcpy(&ram[selected_page][selected_column], data, size);
    selected_column += (unsigned int)size;
    ++data_calls;
    data_bytes += (unsigned int)size;
    return ESP_OK;
}

/** @brief Observe startup stabilization without sleeping or scheduling tasks. */
void vTaskDelay(TickType_t ticks)
{
    assert(display_on);
    delayed_ticks += ticks;
}

#if CONFIG_PROBE_OLED_ENABLE
/** @brief Reset controller observations between initialization attempts without hiding leaks. */
static void reset_controller(void)
{
    assert(!bus_token.owned && !io_token.owned);
    (void)memset(ram, 0xa5, sizeof(ram));
    selected_page = 0U;
    selected_column = 0U;
    pending_argument = 0U;
    dc_dc_enabled = false;
    display_on = false;
    first_display_enable = true;
    /* Start from the previous orientation so omitting either direction command is detected. */
    segment_remapped = true;
    com_reversed = true;
    probes = 0U;
    io_calls = 0U;
    data_calls = 0U;
    data_bytes = 0U;
    fail_io_call = 0U;
    fail_bus = false;
    fail_io_create = false;
    delayed_ticks = 0U;
}

/** @brief Exercise absent hardware, allocation failures and command/data timeout cleanup. */
static void test_initialization_failures(void)
{
    reset_controller();
    fail_bus = true;
    assert(probe_oled_init() == ESP_ERR_NOT_FOUND);
    assert(probes == 0U);
    reset_controller();
    ack_address = 0U;
    assert(probe_oled_init() == ESP_ERR_NOT_FOUND);
    assert(probes == 2U);
    reset_controller();
    ack_address = 0x3cU;
    fail_io_create = true;
    assert(probe_oled_init() == ESP_ERR_NOT_FOUND);
    reset_controller();
    fail_io_call = 1U;
    assert(probe_oled_init() == ESP_ERR_NOT_FOUND);
    reset_controller();
    fail_io_call = 7U;
    assert(probe_oled_init() == ESP_ERR_NOT_FOUND);
    assert(!display_on);
    reset_controller();
    fail_io_call = 18U;
    assert(probe_oled_init() == ESP_ERR_NOT_FOUND);
    assert(!display_on);
    reset_controller();
    assert(probe_oled_draw(NULL, 0U) == ESP_ERR_INVALID_STATE);
    assert(probe_oled_startup_frame(PROBE_OLED_STARTUP_FILL, 0U) == ESP_ERR_INVALID_STATE);
    assert(probe_oled_contrast(128U) == ESP_ERR_INVALID_ARG);
    (void)puts("PASS: SH1106 discovery, failed-stage cleanup and initialization retry");
}

/** @brief Verify all eight visible pages and untouched guard columns for the configured offset. */
static void test_visible_window(void)
{
    static const uint8_t first_columns[] = {0x3eU, 0x40U, 0x62U, 0x22U, 0x18U, 0x27U, 0x3cU, 3U};
    const char rows[8][17] = {"0", "1", "2", "3", "4", "5", "6", "7"};
    (void)memset(ram, 0xa5, sizeof(ram));
    data_calls = 0U;
    data_bytes = 0U;
    assert(probe_oled_draw(rows, 8U) == ESP_OK);
    assert(data_calls == 8U && data_bytes == 1024U);
    for (size_t page = 0U; page < 8U; ++page)
    {
        const size_t offset = CONFIG_PROBE_OLED_COLUMN_OFFSET;
        assert(ram[page][offset] == first_columns[page]);
        for (size_t column = 0U; column < 132U; ++column)
        {
            if ((column < offset) || (column >= offset + 128U))
            {
                assert(ram[page][column] == 0xa5U);
            }
            else if (column >= offset + 8U)
            {
                assert(ram[page][column] == 0U);
            }
        }
    }
    assert(probe_oled_draw(rows, 0U) == ESP_OK);
    for (size_t page = 0U; page < 8U; ++page)
    {
        for (size_t column = 0U; column < 128U; ++column)
        {
            assert(ram[page][column + CONFIG_PROBE_OLED_COLUMN_OFFSET] == 0U);
        }
    }
    const unsigned int before = io_calls;
    assert(probe_oled_draw(NULL, 1U) == ESP_ERR_INVALID_STATE);
    assert(probe_oled_draw(rows, 9U) == ESP_ERR_INVALID_STATE);
    assert(io_calls == before);
    (void)puts("PASS: eight SH1106 pages, visible-column offset, clearing and input bounds");
}

/** @brief Require immediate error propagation and preserve valid contrast bounds. */
static void test_transfer_errors(void)
{
    const char rows[1][17] = {"SH1106"};
    fail_io_call = io_calls + 1U;
    assert(probe_oled_draw(rows, 1U) == ESP_ERR_TIMEOUT);
    assert(io_calls == fail_io_call);
    fail_io_call = io_calls + 2U;
    assert(probe_oled_draw(rows, 1U) == ESP_ERR_TIMEOUT);
    assert(io_calls == fail_io_call);
    fail_io_call = 0U;
    assert(probe_oled_draw(rows, 1U) == ESP_OK);
    assert(probe_oled_contrast(0U) == ESP_OK && contrast == 0U);
    assert(probe_oled_contrast(255U) == ESP_OK && contrast == 255U);
    const unsigned int before = io_calls;
    assert(probe_oled_contrast(256U) == ESP_ERR_INVALID_ARG && io_calls == before);
    fail_io_call = io_calls + 1U;
    assert(probe_oled_contrast(128U) == ESP_ERR_TIMEOUT);
    fail_io_call = 0U;
    (void)puts("PASS: command/data errors stop transfer and contrast is bounded");
}

/** @brief Copy the visible page bytes independently of the driver's private framebuffer. */
static void visible_image(uint8_t image[1024])
{
    for (size_t page = 0U; page < 8U; ++page)
    {
        (void)memcpy(&image[page * 128U], &ram[page][CONFIG_PROBE_OLED_COLUMN_OFFSET], 128U);
    }
}

/** @brief Inspect one pixel in logical display coordinates, matching the current readable text. */
static bool image_pixel(const uint8_t image[1024], size_t x, size_t y)
{
    return (image[((y / 8U) * 128U) + x] & (uint8_t)(1U << (y % 8U))) != 0U;
}

/** @brief Optionally save actual transmitted pixels for visual inspection outside the test. */
static void save_startup_preview(unsigned int index)
{
    const char *directory = getenv("PROBE_OLED_PREVIEW_DIR");
    if (directory != NULL)
    {
        char path[1024];
        const int written = snprintf(path, sizeof(path), "%s/frame-%02u.pgm", directory, index);
        assert(written > 0 && (size_t)written < sizeof(path));
        FILE *output = fopen(path, "wb");
        assert(output != NULL);
        (void)fprintf(output, "P5\n128 64\n255\n");
        uint8_t pixels[128];
        for (size_t y = 0U; y < 64U; ++y)
        {
            for (size_t x = 0U; x < 128U; ++x)
            {
                const bool lit = (ram[y / 8U][x + CONFIG_PROBE_OLED_COLUMN_OFFSET] &
                                  (uint8_t)(1U << (y % 8U))) != 0U;
                /* Approximate the controller's global contrast for the software preview. */
                const uint32_t brightness = ((uint32_t)contrast + 1U) * 255U / 129U;
                pixels[x] =
                    (display_on && lit) ? (uint8_t)((brightness > 255U) ? 255U : brightness) : 0U;
            }
            assert(fwrite(pixels, 1U, sizeof(pixels), output) == sizeof(pixels));
        }
        assert(fclose(output) == 0);
    }
}

/** @brief Verify the complete 16x8 cell grid, case, punctuation and composed settings frames. */
static void test_settings_display(void)
{
    const char cells[3][17] = {"0000000000000000", "_e[]%", "\x01\x02\xff"};
    /* Independent golden bytes from the owner's supplied font, including all eight columns. */
    const uint8_t zero[] = {0x3eU, 0x7fU, 0x71U, 0x59U, 0x4dU, 0x7fU, 0x3eU, 0U};
    const uint8_t up[] = {0U, 4U, 2U, 0xffU, 2U, 4U, 0U, 0U};
    const uint8_t down[] = {0U, 0x20U, 0x40U, 0xffU, 0x40U, 0x20U, 0U, 0U};
    const size_t offset = CONFIG_PROBE_OLED_COLUMN_OFFSET;
    assert(probe_oled_draw(cells, 3U) == ESP_OK);
    for (size_t cell = 0U; cell < 16U; ++cell)
    {
        assert(memcmp(&ram[0][offset + cell * 8U], zero, sizeof(zero)) == 0);
    }
    for (size_t column = 0U; column < 8U; ++column)
    {
        assert(ram[1][offset + column] == 0x80U);    /* Underscore uses the last pixel row. */
        assert(ram[2][offset + 16U + column] == 0U); /* High bytes must not index the font. */
    }
    assert(ram[1][offset + 8U] == 0x38U); /* Preserve lowercase 'e', rather than uppercase 'E'. */
    assert(ram[1][offset + 9U] == 0x7cU);
    assert(ram[1][offset + 16U + 2U] == 0x7fU);
    assert(ram[1][offset + 24U + 4U] == 0x7fU);
    assert(ram[1][offset + 32U] == 0x46U);
    assert(memcmp(&ram[2][offset], up, sizeof(up)) == 0);
    assert(memcmp(&ram[2][offset + 8U], down, sizeof(down)) == 0);
    probe_ui_settings_t settings;
    probe_ui_settings_init(&settings);
    char rows[8][17];
    const probe_ui_settings_action_t actions[] = {
        PROBE_UI_SETTINGS_SELECT,   PROBE_UI_SETTINGS_NEXT,     PROBE_UI_SETTINGS_SELECT,
        PROBE_UI_SETTINGS_NEXT,     PROBE_UI_SETTINGS_SELECT,   PROBE_UI_SETTINGS_NEXT,
        PROBE_UI_SETTINGS_SELECT,   PROBE_UI_SETTINGS_NEXT,     PROBE_UI_SETTINGS_SELECT,
        PROBE_UI_SETTINGS_CANCEL,   PROBE_UI_SETTINGS_NEXT,     PROBE_UI_SETTINGS_SELECT,
        PROBE_UI_SETTINGS_PREVIOUS, PROBE_UI_SETTINGS_SELECT,   PROBE_UI_SETTINGS_NEXT,
        PROBE_UI_SETTINGS_SELECT,   PROBE_UI_SETTINGS_NEXT,     PROBE_UI_SETTINGS_SELECT,
        PROBE_UI_SETTINGS_NEXT,     PROBE_UI_SETTINGS_SELECT,   PROBE_UI_SETTINGS_NEXT,
        PROBE_UI_SETTINGS_NEXT,     PROBE_UI_SETTINGS_SELECT,   PROBE_UI_SETTINGS_NEXT,
        PROBE_UI_SETTINGS_SELECT,   PROBE_UI_SETTINGS_PREVIOUS, PROBE_UI_SETTINGS_CANCEL,
        PROBE_UI_SETTINGS_NEXT,     PROBE_UI_SETTINGS_SELECT,   PROBE_UI_SETTINGS_SELECT,
        PROBE_UI_SETTINGS_PREVIOUS, PROBE_UI_SETTINGS_SELECT,   PROBE_UI_SETTINGS_PREVIOUS,
        PROBE_UI_SETTINGS_SELECT};
    for (size_t step = 0U; step <= sizeof(actions) / sizeof(actions[0]); ++step)
    {
        if (step > 0U)
        {
            assert(probe_ui_settings_input(&settings, actions[step - 1U]));
        }
        probe_ui_settings_render(&settings, rows);
        const unsigned int value =
            ((unsigned int)probe_ui_settings_oled_percent(&settings) * 255U + 50U) / 100U;
        assert(probe_oled_contrast(value) == ESP_OK);
        assert(probe_oled_draw(rows, 8U) == ESP_OK);
        save_startup_preview(100U + (unsigned int)step);
        if (settings.page == PROBE_UI_SETTINGS_HOME)
        {
            for (size_t column = 0U; column < 128U; ++column)
            {
                assert(ram[1][offset + column] == 0U); /* One complete blank row under Settings. */
            }
            for (size_t column = 0U; column < 32U; ++column)
            {
                assert(ram[0][offset + column] == 0U);
                assert(ram[0][offset + 96U + column] == 0U);
            }
            assert((ram[0][offset + 80U] & 0x80U) != 0U); /* Header 'g' retains its descender. */
        }
    }
    (void)puts("PASS: owner 8x8 font, 16 cells, arrows, descenders, high bytes and menu frames");
}

/** @brief Render the SWD detail page through actual SH1106 transfers, including locked reset width.
 */
static void test_swd_display(void)
{
    probe_ui_settings_t settings;
    open_swd_settings(&settings);
    const probe_ui_settings_action_t actions[] = {
        PROBE_UI_SETTINGS_SELECT, PROBE_UI_SETTINGS_NEXT,   PROBE_UI_SETTINGS_SELECT,
        PROBE_UI_SETTINGS_NEXT,   PROBE_UI_SETTINGS_SELECT, PROBE_UI_SETTINGS_PREVIOUS,
        PROBE_UI_SETTINGS_SELECT, PROBE_UI_SETTINGS_NEXT,   PROBE_UI_SETTINGS_PREVIOUS,
        PROBE_UI_SETTINGS_SELECT, PROBE_UI_SETTINGS_NEXT,   PROBE_UI_SETTINGS_SELECT,
        PROBE_UI_SETTINGS_NEXT,   PROBE_UI_SETTINGS_SELECT, PROBE_UI_SETTINGS_NEXT,
        PROBE_UI_SETTINGS_SELECT, PROBE_UI_SETTINGS_NEXT,   PROBE_UI_SETTINGS_SELECT,
        PROBE_UI_SETTINGS_NEXT,   PROBE_UI_SETTINGS_SELECT};
    for (size_t step = 0U; step <= sizeof(actions) / sizeof(actions[0]); ++step)
    {
        if (step > 0U)
        {
            assert(probe_ui_settings_input(&settings, actions[step - 1U]));
        }
        char rows[8][17];
        probe_ui_settings_render(&settings, rows);
        check_settings_layout(&settings);
        assert(probe_oled_draw(rows, 8U) == ESP_OK);
        save_startup_preview(200U + (unsigned int)step);
    }
    check_swd_config(&settings, 1000000U, true, 200U, true);
    (void)puts("PASS: SWD page pixels, four-digit frequency, draft brackets and disabled Reset ms");
}

/** @brief Render live Wi-Fi snapshots with fixture credentials only, never real AP secrets. */
static void test_wifi_display(void)
{
    probe_ui_settings_t settings;
    open_wifi_settings(&settings);
    probe_ui_wifi_view_t view = {.station_ssid = "LAB-ROUTER",
                                 .ap_ssid = "MAGIC-PROBE",
                                 .ap_password = "1234567890",
                                 .portal_ready = true,
                                 .station_connected = true};
    for (unsigned int step = 0U; step < 6U; ++step)
    {
        if (step == 1U)
        {
            view.setup_enabled = true;
            view.station_connected = true;
        }
        else if (step == 2U)
        {
            view.setup_enabled = false;
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
        }
        else if (step == 3U)
        {
            view.station_connected = false;
        }
        else if (step == 4U)
        {
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
            view.station_connected = true;
            view.setup_enabled = true;
            (void)memcpy(view.station_ssid, "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456", 33U);
        }
        else if (step == 5U)
        {
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
            assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
        }
        (void)probe_ui_settings_wifi_update(&settings, &view);
        char rows[8][17];
        probe_ui_settings_render(&settings, rows);
        check_settings_layout(&settings);
        assert(probe_oled_draw(rows, 8U) == ESP_OK);
        save_startup_preview(300U + step);
    }
    (void)puts("PASS: spaced WiFi frames, OFF hides IP/AP/PW, NONE and long station names");
}

/** @brief Validate exact endpoints, diagonal direction, monotonic masks and title geometry. */
static void test_startup(void)
{
    uint8_t logo[1024];
    uint8_t previous[1024];
    uint8_t current[1024];
    assert(probe_oled_startup_frame(PROBE_OLED_STARTUP_REVEAL, 255U) == ESP_OK);
    visible_image(logo);
    const size_t lengths[] = {5U, 8U, 4U};
    const size_t lefts[] = {24U, 0U, 32U};
    for (size_t y = 0U; y < 64U; ++y)
    {
        for (size_t x = 0U; x < 128U; ++x)
        {
            bool in_word = false;
            for (size_t line = 0U; line < 3U; ++line)
            {
                const size_t top = 5U + line * 20U;
                in_word = in_word || ((y >= top) && (y < top + 16U) && (x >= lefts[line]) &&
                                      (x < lefts[line] + lengths[line] * 16U));
            }
            if (!in_word)
            {
                assert(!image_pixel(logo, x, y));
            }
        }
    }
    for (size_t line = 0U; line < 3U; ++line)
    {
        for (size_t letter = 0U; letter < lengths[line]; ++letter)
        {
            bool nonempty = false;
            for (size_t y = 0U; y < 16U; y += 2U)
            {
                for (size_t x = 0U; x < 16U; x += 2U)
                {
                    const size_t px = lefts[line] + (letter * 16U) + x;
                    const size_t py = 5U + (line * 20U) + y;
                    const bool value = image_pixel(logo, px, py);
                    assert(image_pixel(logo, px + 1U, py) == value);
                    assert(image_pixel(logo, px, py + 1U) == value);
                    assert(image_pixel(logo, px + 1U, py + 1U) == value);
                    nonempty = nonempty || value;
                }
            }
            assert(nonempty);
        }
    }
    assert(probe_oled_startup_frame(PROBE_OLED_STARTUP_FILL, 0U) == ESP_OK);
    visible_image(previous);
    assert(!display_on);
    for (size_t index = 0U; index < sizeof(previous); ++index)
    {
        assert(previous[index] == 0U);
    }
    save_startup_preview(0U);
    uint8_t previous_contrast = contrast;
    for (uint32_t step = 1U; step <= 16U; ++step)
    {
        assert(probe_oled_startup_frame(PROBE_OLED_STARTUP_FILL, (uint8_t)((step * 255U) / 16U)) ==
               ESP_OK);
        visible_image(current);
        assert(display_on && contrast >= previous_contrast);
        for (size_t index = 0U; index < sizeof(current); ++index)
        {
            assert((previous[index] & (uint8_t)~current[index]) == 0U);
        }
        if (step == 8U)
        {
            assert(image_pixel(current, 0U, 0U));
            assert(!image_pixel(current, 127U, 63U));
        }
        previous_contrast = contrast;
        (void)memcpy(previous, current, sizeof(previous));
        save_startup_preview((unsigned int)step);
    }
    for (size_t index = 0U; index < sizeof(current); ++index)
    {
        assert(current[index] == 0xffU);
    }
    for (uint32_t step = 1U; step <= 16U; ++step)
    {
        assert(probe_oled_startup_frame(PROBE_OLED_STARTUP_REVEAL,
                                        (uint8_t)((step * 255U) / 16U)) == ESP_OK);
        visible_image(current);
        for (size_t index = 0U; index < sizeof(current); ++index)
        {
            assert((current[index] & (uint8_t)~previous[index]) == 0U);
            assert((current[index] & logo[index]) == logo[index]);
        }
        if (step == 8U)
        {
            assert(!image_pixel(current, 0U, 0U));
            assert(image_pixel(current, 127U, 63U));
        }
        (void)memcpy(previous, current, sizeof(previous));
        save_startup_preview((unsigned int)step + 16U);
    }
    assert(memcmp(current, logo, sizeof(logo)) == 0);
    assert(contrast == 128U && !segment_remapped && !com_reversed);
    const unsigned int before = io_calls;
    assert(probe_oled_startup_frame((probe_oled_startup_phase_t)99, 0U) == ESP_ERR_INVALID_STATE);
    assert(io_calls == before);
    for (unsigned int failure = 1U; failure <= 4U; ++failure)
    {
        fail_io_call = io_calls + failure;
        assert(probe_oled_startup_frame(PROBE_OLED_STARTUP_REVEAL, 128U) == ESP_ERR_TIMEOUT);
        assert(io_calls == fail_io_call);
    }
    fail_io_call = 0U;
    (void)puts(
        "PASS: diagonal startup, exact endpoints, preserved three-line wordmark and I/O errors");
}
#endif

/** @brief Run the controller-model suite or prove the disabled build performs no I/O. */
int main(void)
{
    test_settings_model();
#if CONFIG_PROBE_OLED_ENABLE
    test_initialization_failures();
    ack_address = 0x3dU;
    assert(probe_oled_init() == ESP_OK && probes == 2U);
    assert(display_on && dc_dc_enabled && pending_argument == 0U);
    assert(!segment_remapped && !com_reversed);
    (void)puts("PASS: segment and COM directions apply the requested 180-degree rotation");
    assert(contrast == 128U && delayed_ticks >= 100U);
    assert(data_calls == 8U && data_bytes == 1056U);
    assert(probe_oled_init() == ESP_ERR_INVALID_STATE);
    test_visible_window();
    test_transfer_errors();
    test_startup();
    test_settings_display();
    test_swd_display();
    test_wifi_display();
#else
    const char rows[1][17] = {"DISABLED"};
    assert(probe_oled_init() == ESP_ERR_NOT_SUPPORTED);
    assert(probe_oled_draw(rows, 1U) == ESP_ERR_INVALID_STATE);
    assert(probe_oled_startup_frame(PROBE_OLED_STARTUP_FILL, 0U) == ESP_ERR_INVALID_STATE);
    assert(probe_oled_contrast(128U) == ESP_ERR_INVALID_ARG);
    assert(!bus_token.owned && !io_token.owned && probes == 0U && io_calls == 0U);
    (void)puts("PASS: disabled OLED performs no bus or panel operations");
#endif
    return 0;
}
