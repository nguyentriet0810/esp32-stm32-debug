/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_encoder_decode.h @brief Pure full-step decoder; AB=3 is the idle detent. */
#ifndef PROBE_ENCODER_DECODE_H
#define PROBE_ENCODER_DECODE_H
#include <stdint.h>
typedef struct
{
    uint8_t previous;
    int8_t accumulator;
} probe_encoder_decoder_t;
/**
 * @brief Feed one 2-bit AB sample; four valid edges returning to AB=3 form one detent.
 * @param[in,out] decoder Caller-owned state, initially previous=3, accumulator=0.
 * @param[in] sample Current AB code, 0..3.
 * @return -1/0/+1 menu detents; NULL/invalid input returns zero. No allocation or I/O.
 * @note Caller must serialize access to each decoder instance.
 */
int8_t probe_encoder_decode(probe_encoder_decoder_t *decoder, uint8_t sample);
#endif
