/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/** @file encoder_test.c @brief Host tests for full-step, bounce and corrupted input. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "probe_encoder_decode.h"
static int run(const uint8_t *samples, size_t count)
{
    probe_encoder_decoder_t decoder = {.previous = 3U, .accumulator = 0};
    int result = 0;
    for (size_t index = 0U; index < count; index++)
    {
        result += probe_encoder_decode(&decoder, samples[index]);
    }
    return result;
}
int main(void)
{
    const uint8_t positive[] = {3U, 1U, 0U, 2U, 3U};
    const uint8_t negative[] = {3U, 2U, 0U, 1U, 3U};
    const uint8_t bounce[] = {3U, 1U, 3U, 1U, 0U, 2U, 0U, 2U, 3U};
    const uint8_t partial[] = {3U, 1U, 0U, 1U, 3U};
    const uint8_t jump[] = {3U, 0U, 1U, 3U};
    const uint8_t idle[] = {3U, 3U, 3U, 3U};
    assert(run(positive, sizeof(positive)) == 1);
    assert(run(negative, sizeof(negative)) == -1);
    assert(run(bounce, sizeof(bounce)) == 1);
    assert(run(partial, sizeof(partial)) == 0);
    assert(run(jump, sizeof(jump)) == 0);
    assert(run(idle, sizeof(idle)) == 0);
    assert(probe_encoder_decode(NULL, 0U) == 0);
    probe_encoder_decoder_t damaged = {.previous = 255U, .accumulator = 127};
    assert(probe_encoder_decode(&damaged, 1U) == 0);
    assert(damaged.previous == 1U);
    assert(probe_encoder_decode(&damaged, 255U) == 0);
    uint32_t seed = 1U;
    for (size_t index = 0U; index < 10000U; index++)
    {
        seed = (seed * UINT32_C(1664525)) + UINT32_C(1013904223);
        const int8_t result = probe_encoder_decode(&damaged, (uint8_t)(seed >> 24U));
        assert((result >= -1) && (result <= 1));
    }
    (void)puts("PASS: encoder direction, bounce, partial, invalid input and 10000 samples");
    return 0;
}
