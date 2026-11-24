/*
 * SPDX-FileCopyrightText: 2026 Piotr Wasilewski
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * newlib integer-only stdio entry points for PicolibC builds, see libc_compat.h.
 */

/**
 * @file libc_compat.c
 * @brief Newlib-compatible stdio entry points for the BMD adapter.
 *
 * Forwards integer-variant symbol names to the configured libc stdio implementation. Formatting,
 * allocation and error results follow that implementation.
 */

#include "libc_compat.h"

#include <stdio.h>

/**
 * @brief Format into an unbounded destination through the configured libc.
 * @param[out] str Non-NULL caller-owned output with sufficient capacity.
 * @param[in] format Non-NULL printf-style format followed by matching variadic arguments.
 * @return Formatted character count, excluding the terminator, or a negative error.
 * @note Preserves the legacy unbounded ABI; capacity is the caller's responsibility.
 */
int siprintf(char *const str, const char *const format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = vsprintf(str, format, args);
    va_end(args);
    return result;
}

/**
 * @brief Format into a bounded destination through the configured libc.
 * @param[out] str Caller-owned output of size bytes; may be NULL only when size is zero.
 * @param[in] size Destination capacity in bytes, including space for the terminator.
 * @param[in] format Non-NULL printf-style format followed by matching variadic arguments.
 * @return Required character count excluding the terminator, or a negative error.
 * @note A nonnegative result at least size indicates truncation.
 */
int sniprintf(char *const str, const size_t size, const char *const format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}

/**
 * @brief Forward a variadic-list format request to bounded libc formatting.
 * @param[out] str Caller-owned output; may be NULL only when size is zero.
 * @param[in] size Destination capacity in bytes, including the terminator.
 * @param[in] format Non-NULL printf-style format string.
 * @param[in] args Initialized argument list matching format; apply normal va_list rules.
 * @return Required character count excluding the terminator, or a negative error.
 */
int vsniprintf(char *const str, const size_t size, const char *const format, va_list args)
{
    return vsnprintf(str, size, format, args);
}

/**
 * @brief Allocate formatted text using the configured libc allocation convention.
 * @param[out] str Non-NULL output pointer; on success the caller frees the allocation.
 * @param[in] format Non-NULL printf-style format string.
 * @param[in] args Initialized argument list matching format; apply normal va_list rules.
 * @return Formatted character count excluding the terminator, or a negative error.
 * @note Output-pointer behavior on failure follows the configured libc implementation.
 */
int vasiprintf(char **const str, const char *const format, va_list args)
{
    return vasprintf(str, format, args);
}

/**
 * @brief Parse a string using the configured libc scanf implementation.
 * @param[in] str NUL-terminated input string.
 * @param[in] format Non-NULL scanf-style format followed by matching output arguments.
 * @return Number of assignments, or EOF when libc reports an input failure.
 * @note Format widths and destination capacities remain the caller's responsibility.
 */
int siscanf(const char *const str, const char *const format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = vsscanf(str, format, args);
    va_end(args);
    return result;
}
