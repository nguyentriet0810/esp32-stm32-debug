/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_encoder_decode.c @brief Gray-code validation rejects two-bit jumps. */
#include "probe_encoder_decode.h"
#include <stddef.h>
int8_t probe_encoder_decode(probe_encoder_decoder_t *decoder, uint8_t sample)
{
    static const int8_t transitions[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    int8_t detent = 0;
    if ((decoder != NULL) && (sample <= 3U))
    {
        const uint8_t previous = decoder->previous;
        if ((previous > 3U) || (decoder->accumulator > 3) || (decoder->accumulator < -3))
        {
            decoder->accumulator = 0;
        }
        else if ((previous ^ sample) == 3U)
        {
            decoder->accumulator = 0;
        }
        else
        {
            const size_t index = ((size_t)previous * 4U) + sample;
            decoder->accumulator = (int8_t)(decoder->accumulator + transitions[index]);
        }
        decoder->previous = sample;
        if (sample == 3U)
        {
            if (decoder->accumulator >= 4)
            {
                detent = 1;
            }
            else if (decoder->accumulator <= -4)
            {
                detent = -1;
            }
            decoder->accumulator = 0;
        }
    }
    return detent;
}
