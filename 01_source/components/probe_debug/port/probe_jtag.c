/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file probe_jtag.c
 * @brief Empty-chain JTAG compatibility callbacks.
 *
 * The PCB target interface uses SWD. These callbacks report pulled-high scan data without
 * driving a JTAG interface, allowing the backend to handle an empty chain.
 */
#include <string.h>
#include "general.h"
#include "jtagtap.h"
/**
 * @brief Leave hardware untouched when the backend resets an unsupported JTAG chain.
 */
static void tap_reset(void)
{
}
/**
 * @brief Model an empty JTAG chain with pulled-high TDO.
 * @param[in] tms Ignored mode-select bit.
 * @param[in] tdi Ignored output bit.
 * @return true; no physical clock is generated.
 */
static bool tap_next(bool tms, bool tdi)
{
    (void)tms;
    (void)tdi;
    return true;
}
/**
 * @brief Discard a TMS sequence because this target interface has no JTAG transport.
 */
static void tap_tms(uint32_t states, size_t cycles)
{
    (void)states;
    (void)cycles;
}
/**
 * @brief Fill captured scan bytes with ones without driving target GPIO.
 * @param[out] out Optional buffer of at least (cycles + 7) / 8 bytes.
 * @param[in] final_tms Ignored final TMS state.
 * @param[in] in Ignored outgoing bit sequence.
 * @param[in] cycles Number of scan bits represented in the output buffer.
 */
static void tap_both(uint8_t *out, bool final_tms, const uint8_t *in, size_t cycles)
{
    (void)final_tms;
    (void)in;
    if (out != NULL)
    {
        (void)memset(out, 0xff, (cycles + 7U) / 8U);
    }
}
/**
 * @brief Discard output-only scan bits without accessing hardware.
 */
static void tap_out(bool final_tms, const uint8_t *in, size_t cycles)
{
    (void)final_tms;
    (void)in;
    (void)cycles;
}
/**
 * @brief Discard repeated JTAG cycles without accessing hardware.
 */
static void tap_cycle(bool tms, bool tdi, size_t cycles)
{
    (void)tms;
    (void)tdi;
    (void)cycles;
}
jtag_proc_s jtag_proc;
/**
 * @brief Install the empty-chain callback table expected by the BMD scanner.
 */
void jtagtap_init(void)
{
    jtag_proc.jtagtap_reset = tap_reset;
    jtag_proc.jtagtap_next = tap_next;
    jtag_proc.jtagtap_tms_seq = tap_tms;
    jtag_proc.jtagtap_tdi_tdo_seq = tap_both;
    jtag_proc.jtagtap_tdi_seq = tap_out;
    jtag_proc.jtagtap_cycle = tap_cycle;
    jtag_proc.tap_idle_cycles = 1U;
}
