/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file probe_wifi_form.h @brief Bounded form decoding for the local Wi-Fi setup page. */
#ifndef PROBE_WIFI_FORM_H
#define PROBE_WIFI_FORM_H
#include <stdbool.h>
#include <stddef.h>

/** @brief Maximum encoded request size, including percent-encoded UTF-8 credentials. */
#define PROBE_WIFI_FORM_MAX 320U
/** @brief Caller-owned credentials, never published as status or logged. */
typedef struct
{
    char ssid[33];
    char password[64];
} probe_wifi_credentials_t;
/**
 * @brief Decode exactly one ssid and password from a bounded URL-encoded POST body.
 * @param[in] body Borrowed bytes, not required to end in NUL.
 * @param[in] length Body length in bytes, 1..320.
 * @param[out] credentials Non-NULL output, cleared on any rejection.
 * @return True for a 1..32-byte SSID and 8..63-byte WPA password.
 * @note Pure task-context function; rejects duplicate/unknown fields, malformed escapes and NUL.
 */
bool probe_wifi_form_parse(const char *body, size_t length, probe_wifi_credentials_t *credentials);
/**
 * @brief Escape a terminated UTF-8 string for a JSON string value, without surrounding quotes.
 * @param[in] input Non-NULL terminated input.
 * @param[out] output Caller-owned destination, never overlaps input.
 * @param[in] capacity Destination bytes including NUL.
 * @return True if the complete escaped string fits; false otherwise.
 */
bool probe_wifi_json_escape(const char *input, char *output, size_t capacity);
#endif
