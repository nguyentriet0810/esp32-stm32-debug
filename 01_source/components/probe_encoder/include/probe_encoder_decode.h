/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_encoder_decode.h
 * @brief Platform-independent quadrature decoder contract.
 *
 * Each caller owns and serializes its decoder state. Samples contain two AB bits; output is a
 * signed full-detent count.
 */
#ifndef PROBE_ENCODER_DECODE_H
#define PROBE_ENCODER_DECODE_H
#include <stdint.h>
/** @brief Caller-owned history for one independent quadrature input. */
typedef struct
{
    uint8_t previous;   /**< Previous AB sample; initialize to idle code 3. */
    int8_t accumulator; /**< Signed edge count since the last detent; initialize to 0. */
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
