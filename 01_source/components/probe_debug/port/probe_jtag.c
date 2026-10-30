/*
 * SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/** @file probe_jtag.c @brief Empty-chain hooks; this board supports SWD only. */
#include <string.h>
#include "general.h"
#include "jtagtap.h"
static void tap_reset(void)
{
}
static bool tap_next(bool tms, bool tdi)
{
    (void)tms;
    (void)tdi;
    return true;
}
static void tap_tms(uint32_t states, size_t cycles)
{
    (void)states;
    (void)cycles;
}
static void tap_both(uint8_t *out, bool final_tms, const uint8_t *in, size_t cycles)
{
    (void)final_tms;
    (void)in;
    if (out != NULL)
    {
        (void)memset(out, 0xff, (cycles + 7U) / 8U);
    }
}
static void tap_out(bool final_tms, const uint8_t *in, size_t cycles)
{
    (void)final_tms;
    (void)in;
    (void)cycles;
}
static void tap_cycle(bool tms, bool tdi, size_t cycles)
{
    (void)tms;
    (void)tdi;
    (void)cycles;
}
jtag_proc_s jtag_proc;
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
