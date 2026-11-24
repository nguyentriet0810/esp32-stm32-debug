/*
 * SPDX-FileCopyrightText: 2026 Piotr Wasilewski
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Force-included into every translation unit that can see BMD headers.
 *
 * BMD's stdio_newlib.h redirects sprintf, snprintf, vsnprintf, vasprintf and sscanf to
 * newlib's integer-only variants. ESP-IDF 6 defaults to PicolibC, which has none of them,
 * so they are declared here and implemented in libc_compat.c. The prototypes match
 * newlib's, so this header is harmless with CONFIG_LIBC_NEWLIB as well.
 */

/**
 * @file libc_compat.h
 * @brief C/C++ ABI declarations for BMD stdio compatibility.
 *
 * Force-included by the adapter build before BMD headers rename stdio functions. Preserves the
 * original symbol signatures without importing ESP-IDF configuration macros.
 */

#ifndef LIBC_COMPAT_H
#define LIBC_COMPAT_H

#include <stdarg.h>
#include <stddef.h>

/* The C++ sources include BMD headers too, which maps their stdio calls to these */
#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Format into an unbounded destination through the configured libc.
     * @param[out] str Non-NULL caller-owned output with sufficient capacity.
     * @param[in] format Non-NULL printf-style format followed by matching variadic arguments.
     * @return Formatted character count, excluding the terminator, or a negative error.
     * @note Preserves the legacy unbounded ABI; capacity is the caller's responsibility.
     */
    int siprintf(char *str, const char *format, ...);
    /**
     * @brief Format into a bounded destination through the configured libc.
     * @param[out] str Caller-owned output of size bytes; may be NULL only when size is zero.
     * @param[in] size Destination capacity in bytes, including space for the terminator.
     * @param[in] format Non-NULL printf-style format followed by matching variadic arguments.
     * @return Required character count excluding the terminator, or a negative error.
     * @note A nonnegative result at least size indicates truncation.
     */
    int sniprintf(char *str, size_t size, const char *format, ...);
    /**
     * @brief Forward a variadic-list format request to bounded libc formatting.
     * @param[out] str Caller-owned output; may be NULL only when size is zero.
     * @param[in] size Destination capacity in bytes, including the terminator.
     * @param[in] format Non-NULL printf-style format string.
     * @param[in] args Initialized argument list matching format; apply normal va_list rules.
     * @return Required character count excluding the terminator, or a negative error.
     */
    int vsniprintf(char *str, size_t size, const char *format, va_list args);
    /**
     * @brief Allocate formatted text using the configured libc allocation convention.
     * @param[out] str Non-NULL output pointer; on success the caller frees the allocation.
     * @param[in] format Non-NULL printf-style format string.
     * @param[in] args Initialized argument list matching format; apply normal va_list rules.
     * @return Formatted character count excluding the terminator, or a negative error.
     * @note Output-pointer behavior on failure follows the configured libc implementation.
     */
    int vasiprintf(char **str, const char *format, va_list args);
    /**
     * @brief Parse a string using the configured libc scanf implementation.
     * @param[in] str NUL-terminated input string.
     * @param[in] format Non-NULL scanf-style format followed by matching output arguments.
     * @return Number of assignments, or EOF when libc reports an input failure.
     * @note Format widths and destination capacities remain the caller's responsibility.
     */
    int siscanf(const char *str, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* LIBC_COMPAT_H */
