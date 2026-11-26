/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file wifi_form_test.c @brief Credential bounds, URL decoding and JSON escaping regressions. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "probe_wifi_form.h"

/** @brief Require complete rejection and removal of every partially decoded credential. */
static void reject_form(const char *form)
{
    probe_wifi_credentials_t credentials;
    const probe_wifi_credentials_t empty = {0};
    (void)memset(&credentials, 0xa5, sizeof(credentials));
    assert(!probe_wifi_form_parse(form, strlen(form), &credentials));
    assert(memcmp(&credentials, &empty, sizeof(empty)) == 0);
}

/** @brief Run valid encodings, invalid requests and exact buffer-boundary cases without SDK I/O. */
int main(void)
{
    probe_wifi_credentials_t credentials;
    const char encoded[] = "password=p%40ss%2B%26word&ssid=Guest+2.4G%2FHome";
    assert(probe_wifi_form_parse(encoded, strlen(encoded), &credentials));
    assert(strcmp(credentials.ssid, "Guest 2.4G/Home") == 0);
    assert(strcmp(credentials.password, "p@ss+&word") == 0);
    const char unicode[] = "ssid=Caf%C3%A9&password=testpass";
    assert(probe_wifi_form_parse(unicode, strlen(unicode), &credentials));
    assert(strcmp(credentials.ssid, "Caf\xc3\xa9") == 0);
    const char *const invalid[] = {"",
                                   "ssid=a",
                                   "password=testpass",
                                   "ssid=&password=testpass",
                                   "ssid=a&password=short",
                                   "ssid=a&ssid=b&password=testpass",
                                   "ssid=a&password=testpass&password=otherpass",
                                   "ssid=a&password=testpass&unknown=x",
                                   "ssid=a&password=testpass&",
                                   "ssid=%00&password=testpass",
                                   "ssid=a&password=test%00pass",
                                   "ssid=%&password=testpass",
                                   "ssid=%2&password=testpass",
                                   "ssid=%zz&password=testpass",
                                   "ssid=a&&password=testpass"};
    for (size_t index = 0U; index < sizeof(invalid) / sizeof(invalid[0]); ++index)
    {
        reject_form(invalid[index]);
    }
    char ssid[34];
    char password[65];
    char form[160];
    (void)memset(ssid, 'S', sizeof(ssid));
    ssid[32] = '\0';
    (void)memset(password, 'P', sizeof(password));
    password[63] = '\0';
    (void)snprintf(form, sizeof(form), "ssid=%s&password=%s", ssid, password);
    assert(probe_wifi_form_parse(form, strlen(form), &credentials));
    ssid[32] = 'S';
    ssid[33] = '\0';
    (void)snprintf(form, sizeof(form), "ssid=%s&password=%s", ssid, password);
    reject_form(form);
    ssid[32] = '\0';
    password[63] = 'P';
    password[64] = '\0';
    (void)snprintf(form, sizeof(form), "ssid=%s&password=%s", ssid, password);
    reject_form(form);
    assert(!probe_wifi_form_parse(NULL, 0U, &credentials));
    assert(!probe_wifi_form_parse("x", PROBE_WIFI_FORM_MAX + 1U, &credentials));
    char escaped[64];
    assert(probe_wifi_json_escape("AP\"\\\n\x01", escaped, sizeof(escaped)));
    assert(strcmp(escaped, "AP\\\"\\\\\\u000a\\u0001") == 0);
    char guarded[4] = {'A', 'A', 'A', 'Z'};
    assert(!probe_wifi_json_escape("abcd", guarded, 3U));
    assert(guarded[2] == '\0' && guarded[3] == 'Z');
    assert(probe_wifi_json_escape("ab", guarded, 3U));
    assert(strcmp(guarded, "ab") == 0);
    assert(!probe_wifi_json_escape("a", guarded, 0U));
    (void)puts(
        "PASS: Wi-Fi form encoding, UTF-8, duplicate/NUL rejection, bounds and JSON escaping");
    return 0;
}
