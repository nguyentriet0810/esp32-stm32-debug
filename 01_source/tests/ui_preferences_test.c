/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file ui_preferences_test.c @brief Test OLED NVS persistence, drafts, defaults and failures. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nvs.h"
#include "probe_ui_preferences.h"
#include "probe_ui_settings.h"

static bool namespace_exists;
static bool key_exists;
static bool wrong_type;
static bool opened;
static bool staged;
static uint8_t stored_value;
static uint8_t staged_value;
static nvs_open_mode_t access_mode;
static unsigned int writes;
static unsigned int commits;
static unsigned int closes;
static esp_err_t fail_open;
static esp_err_t fail_get;
static esp_err_t fail_set;
static esp_err_t fail_commit;

/** @brief Enforce namespace isolation and fail missing read-only namespaces like the SDK. */
esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *handle)
{
    assert(strcmp(name, "probe_ui") == 0 && !opened);
    if (fail_open != ESP_OK)
    {
        return fail_open;
    }
    if ((mode == NVS_READONLY) && !namespace_exists)
    {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    namespace_exists = true;
    opened = true;
    access_mode = mode;
    *handle = 1U;
    return ESP_OK;
}

/** @brief Return persisted typed data; rejected reads do not modify the caller's output. */
esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *value)
{
    assert(opened && handle == 1U && strcmp(key, "oled_pct") == 0);
    if (fail_get != ESP_OK)
    {
        return fail_get;
    }
    if (wrong_type)
    {
        return ESP_ERR_NVS_TYPE_MISMATCH;
    }
    if (!key_exists)
    {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    *value = stored_value;
    return ESP_OK;
}

/** @brief Stage one changed byte and count write attempts for preview/wear checks. */
esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value)
{
    assert(opened && handle == 1U && access_mode == NVS_READWRITE);
    assert(strcmp(key, "oled_pct") == 0);
    ++writes;
    if (fail_set == ESP_OK)
    {
        staged_value = value;
        staged = true;
    }
    return fail_set;
}

/** @brief Only a successful commit changes data retained across simulated UI restarts. */
esp_err_t nvs_commit(nvs_handle_t handle)
{
    assert(opened && handle == 1U && staged);
    ++commits;
    if (fail_commit == ESP_OK)
    {
        stored_value = staged_value;
        key_exists = true;
        wrong_type = false;
        staged = false;
    }
    return fail_commit;
}

/** @brief Require every successful open to be closed, including failed operations. */
void nvs_close(nvs_handle_t handle)
{
    assert(opened && handle == 1U);
    opened = false;
    staged = false;
    ++closes;
}

/** @brief Recreate the startup load/validation flow with default state from the real model. */
static void restore_model(probe_ui_settings_t *settings)
{
    probe_ui_settings_init(settings);
    uint8_t percent = 0U;
    if (probe_ui_preferences_oled_load(&percent) == ESP_OK)
    {
        (void)probe_ui_settings_oled_restore(settings, percent);
    }
}

/** @brief Restore all presets and prove preview/Back do not change the committed save input. */
static void test_persistence(void)
{
    probe_ui_settings_t settings;
    restore_model(&settings);
    assert(probe_ui_settings_oled_percent(&settings) == 50U && writes == 0U && closes == 0U);
    const uint8_t presets[] = {10U, 25U, 50U, 75U, 100U};
    for (size_t index = 0U; index < sizeof(presets); ++index)
    {
        assert(probe_ui_preferences_oled_save(presets[index]) == ESP_OK);
        restore_model(&settings);
        assert(probe_ui_settings_oled_percent(&settings) == presets[index]);
        assert(probe_ui_settings_oled_committed_percent(&settings) == presets[index]);
        const unsigned int before = writes;
        assert(probe_ui_preferences_oled_save(presets[index]) == ESP_OK);
        assert(writes == before);
    }
    assert(probe_ui_preferences_oled_save(75U) == ESP_OK);
    restore_model(&settings);
    for (unsigned int step = 0U; step < 3U; ++step)
    {
        assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    }
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(settings.page == PROBE_UI_SETTINGS_OLED && settings.choice == 3U);
    const unsigned int before = writes;
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_oled_percent(&settings) == 100U);
    assert(probe_ui_settings_oled_committed_percent(&settings) == 75U && writes == before);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_NEXT));
    assert(probe_ui_settings_oled_percent(&settings) == 75U);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_oled_committed_percent(&settings) == 75U && writes == before);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_CANCEL));
    assert(probe_ui_settings_oled_committed_percent(&settings) == 75U && writes == before);
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_PREVIOUS));
    assert(probe_ui_settings_input(&settings, PROBE_UI_SETTINGS_SELECT));
    assert(probe_ui_settings_oled_committed_percent(&settings) == 50U);
    assert(probe_ui_preferences_oled_save(probe_ui_settings_oled_committed_percent(&settings)) ==
           ESP_OK);
    restore_model(&settings);
    assert(probe_ui_settings_oled_percent(&settings) == 50U);
    stored_value = 42U;
    restore_model(&settings);
    assert(probe_ui_settings_oled_percent(&settings) == 50U);
    assert(!probe_ui_settings_oled_restore(&settings, 0U));
    assert(!probe_ui_settings_oled_restore(&settings, 255U));
    assert(probe_ui_settings_oled_committed_percent(&settings) == 50U);
}

/** @brief Fault injection checks output preservation, failure propagation and handle cleanup. */
static void test_errors(void)
{
    assert(probe_ui_preferences_oled_load(NULL) == ESP_ERR_INVALID_ARG);
    uint8_t percent = 99U;
    const unsigned int closed = closes;
    fail_open = ESP_FAIL;
    assert(probe_ui_preferences_oled_load(&percent) == ESP_FAIL && percent == 99U);
    assert(probe_ui_preferences_oled_save(25U) == ESP_FAIL && closes == closed);
    fail_open = ESP_OK;
    fail_get = ESP_FAIL;
    assert(probe_ui_preferences_oled_load(&percent) == ESP_FAIL && percent == 99U);
    const unsigned int before = writes;
    assert(probe_ui_preferences_oled_save(25U) == ESP_FAIL && writes == before);
    fail_get = ESP_OK;
    fail_set = ESP_FAIL;
    assert(probe_ui_preferences_oled_save(25U) == ESP_FAIL && !opened);
    fail_set = ESP_OK;
    fail_commit = ESP_FAIL;
    assert(probe_ui_preferences_oled_save(25U) == ESP_FAIL && !opened);
    fail_commit = ESP_OK;
    wrong_type = true;
    assert(probe_ui_preferences_oled_load(&percent) == ESP_ERR_NVS_TYPE_MISMATCH && percent == 99U);
    assert(probe_ui_preferences_oled_save(25U) == ESP_OK && stored_value == 25U && !wrong_type);
    key_exists = false;
    assert(probe_ui_preferences_oled_load(&percent) == ESP_ERR_NVS_NOT_FOUND && percent == 99U);
    assert(!opened && commits > 0U);
}

/** @brief Run production preferences/model code with an isolated in-memory NVS test double. */
int main(void)
{
    test_persistence();
    test_errors();
    (void)puts(
        "PASS: OLED NVS presets/restart, defaults, draft/Back isolation, dedup and storage faults");
    return 0;
}
