/* SPDX-License-Identifier: GPL-3.0-or-later */
/** @file probe_wifi_form.c @brief Strict bounded URL-form decoding and JSON escaping. */
#include "probe_wifi_form.h"
#include <stdint.h>
#include <string.h>

/** @brief Return one hexadecimal digit or -1 for an invalid escape. */
static int form_hex(char value)
{
    int result = -1;
    if ((value >= '0') && (value <= '9'))
    {
        result = value - '0';
    }
    else if ((value >= 'a') && (value <= 'f'))
    {
        result = value - 'a' + 10;
    }
    else if ((value >= 'A') && (value <= 'F'))
    {
        result = value - 'A' + 10;
    }
    return result;
}

/** @brief Decode one value into fixed storage, retaining SSID spaces and UTF-8 bytes. */
static bool form_value(const char *body, size_t length, char *output, size_t capacity)
{
    size_t used = 0U;
    bool valid = true;
    for (size_t index = 0U; (index < length) && valid; ++index)
    {
        unsigned char value = (unsigned char)body[index];
        if (value == (unsigned char)'%')
        {
            if ((length - index) < 3U)
            {
                valid = false;
            }
            else
            {
                const int high = form_hex(body[index + 1U]);
                const int low = form_hex(body[index + 2U]);
                valid = (high >= 0) && (low >= 0);
                if (valid)
                {
                    value = (unsigned char)((unsigned int)high * 16U + (unsigned int)low);
                    index += 2U;
                }
            }
        }
        else if (value == (unsigned char)'+')
        {
            value = (unsigned char)' ';
        }
        if (valid && ((value == 0U) || (used >= capacity - 1U)))
        {
            valid = false;
        }
        if (valid)
        {
            output[used++] = (char)value;
        }
    }
    output[used] = '\0';
    return valid;
}

/** @brief Parse the complete two-field form without allocation or retaining secret input. */
bool probe_wifi_form_parse(const char *body, size_t length, probe_wifi_credentials_t *credentials)
{
    (void)memset(credentials, 0, sizeof(*credentials));
    bool valid = (body != NULL) && (length > 0U) && (length <= PROBE_WIFI_FORM_MAX);
    unsigned int seen = 0U;
    size_t offset = 0U;
    while (valid && (offset < length))
    {
        size_t end = offset;
        while ((end < length) && (body[end] != '&'))
        {
            ++end;
        }
        const size_t part_length = end - offset;
        if ((part_length >= 5U) && (memcmp(&body[offset], "ssid=", 5U) == 0) && ((seen & 1U) == 0U))
        {
            valid = form_value(&body[offset + 5U], part_length - 5U, credentials->ssid,
                               sizeof(credentials->ssid));
            seen |= 1U;
        }
        else if ((part_length >= 9U) && (memcmp(&body[offset], "password=", 9U) == 0) &&
                 ((seen & 2U) == 0U))
        {
            valid = form_value(&body[offset + 9U], part_length - 9U, credentials->password,
                               sizeof(credentials->password));
            seen |= 2U;
        }
        else
        {
            valid = false;
        }
        if ((end < length) && ((end + 1U) == length))
        {
            valid = false;
        }
        offset = end + 1U;
    }
    const size_t password_length = strlen(credentials->password);
    valid = valid && (seen == 3U) && (credentials->ssid[0] != '\0') && (password_length >= 8U) &&
            (password_length <= 63U);
    if (!valid)
    {
        (void)memset(credentials, 0, sizeof(*credentials));
    }
    return valid;
}

/** @brief Encode JSON metacharacters/control bytes while preserving UTF-8 sequences. */
bool probe_wifi_json_escape(const char *input, char *output, size_t capacity)
{
    static const char digits[] = "0123456789abcdef";
    size_t used = 0U;
    bool valid = capacity > 0U;
    for (size_t index = 0U; valid && (input[index] != '\0'); ++index)
    {
        const unsigned char value = (unsigned char)input[index];
        const size_t required =
            (value < 32U) ? 6U : (((value == '"') || (value == '\\')) ? 2U : 1U);
        if (required >= capacity - used)
        {
            valid = false;
        }
        else if (value < 32U)
        {
            (void)memcpy(&output[used], "\\u00", 4U);
            output[used + 4U] = digits[value >> 4U];
            output[used + 5U] = digits[value & 15U];
            used += 6U;
        }
        else
        {
            if ((value == '"') || (value == '\\'))
            {
                output[used++] = '\\';
            }
            output[used++] = (char)value;
        }
    }
    if (capacity > 0U)
    {
        output[used] = '\0';
    }
    return valid;
}
