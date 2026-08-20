// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

/**
 * @file
 * @brief `TimeSpan`'s exact-format scanner -- ticket #1943.
 *
 * **THIS IS A SEPARATE GRAMMAR FROM `InvariantExactDateTimeParser.hpp`, AND REUSING THAT ONE
 * WOULD BE WRONG IN A WAY THAT PASSES MOST TESTS.** .NET keeps `TimeSpanParse.TryParseByFormat`
 * apart from `DateTimeParse` for reasons that are visible in the token table:
 *
 *   * **An unquoted literal is an ERROR here.** `TryParseByFormat`'s `switch` ends in
 *     `default: return result.SetInvalidStringFailure();`, so `"hh:mm"` is **not** a valid
 *     `TimeSpan` format -- the colon must be quoted (`"hh':'mm"`) or escaped (`"hh\\:mm"`). The
 *     date/time scanner matches an unquoted literal against the input instead, so a shared scanner
 *     would silently ACCEPT a format .NET rejects.
 *   * **There is no sign token at all** -- no `-`, no `+`. A negative result comes only from
 *     `TimeSpanStyles::AssumeNegative`, which is what makes that style load-bearing rather than
 *     decorative.
 *   * **Each component may appear at most once**, tracked by five `seen*` flags.
 *   * `d` takes 1..8 specifiers and its digit rule is not the date scanner's: `tokenLen < 2` means
 *     **1 to 8** digits, otherwise **exactly** `tokenLen`.
 *   * `f` requires its digits where `F` makes them optional -- .NET calls `ParseExactDigits` for
 *     both and simply **ignores the result** for `F`.
 *
 * The ranges are .NET's own constants (`TimeSpanParse.cs:59-62`): days <= 10675199, hours <= 23,
 * minutes <= 59, seconds <= 59. They bound each COMPONENT, so `"25"` against `"hh"` fails rather
 * than carrying into days.
 */

#include <cstddef>
#include <cstdint>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::detail {

    /** @brief The components an exact `TimeSpan` format may bind. */
    struct ExactTimeSpanFields {
        std::int64_t days = 0, hours = 0, minutes = 0, seconds = 0, fractionTicks = 0;
    };

    namespace exact_timespan {

        /** @brief How many times the specifier at @p i repeats. .NET's `ParseRepeatPattern`. */
        inline int repeatCount(const std::string& format, std::size_t i, char c) {
            int n = 0;
            while (i + static_cast<std::size_t>(n) < format.size() &&
                   format[i + static_cast<std::size_t>(n)] == c) {
                ++n;
            }
            return n;
        }

        /** @brief Reads between @p minDigits and @p maxDigits digits, counting leading zeroes. */
        inline bool takeDigits(const std::string& input, std::size_t& pos, int minDigits,
                               int maxDigits, std::int64_t& value, int& digitsRead) {
            std::int64_t accumulated = 0;
            digitsRead = 0;
            while (pos < input.size() && digitsRead < maxDigits &&
                   input[pos] >= '0' && input[pos] <= '9') {
                accumulated = accumulated * 10 + (input[pos] - '0');
                ++pos;
                ++digitsRead;
            }
            if (digitsRead < minDigits) return false;
            value = accumulated;
            return true;
        }

    } // namespace exact_timespan

    /**
     * @brief Matches @p input against a CUSTOM `TimeSpan` format, .NET's `TryParseByFormat`.
     *
     * @return false for any mismatch, a repeated component, an out-of-range component, an
     *         unquoted literal, or input left over at the end.
     */
    [[nodiscard]] inline bool MatchExactTimeSpanFormat(const std::string& input,
                                                       const std::string& format,
                                                       ExactTimeSpanFields& fields) {
        using namespace exact_timespan;
        bool seenDays = false, seenHours = false, seenMinutes = false;
        bool seenSeconds = false, seenFraction = false;

        std::size_t pos = 0;   // input cursor
        std::size_t i   = 0;   // format cursor
        int digitsRead  = 0;

        while (i < format.size()) {
            const char c = format[i];
            int tokenLen = 0;

            if (c == 'h' || c == 'm' || c == 's') {
                tokenLen = repeatCount(format, i, c);
                bool& seen = (c == 'h') ? seenHours : (c == 'm') ? seenMinutes : seenSeconds;
                std::int64_t& out = (c == 'h') ? fields.hours
                                  : (c == 'm') ? fields.minutes : fields.seconds;
                if (tokenLen > 2 || seen) return false;
                if (!takeDigits(input, pos, tokenLen, tokenLen, out, digitsRead)) return false;
                seen = true;
            } else if (c == 'd') {
                tokenLen = repeatCount(format, i, c);
                if (tokenLen > 8 || seenDays) return false;
                const int minDigits = tokenLen < 2 ? 1 : tokenLen;
                const int maxDigits = tokenLen < 2 ? 8 : tokenLen;
                if (!takeDigits(input, pos, minDigits, maxDigits, fields.days, digitsRead))
                    return false;
                seenDays = true;
            } else if (c == 'f' || c == 'F') {
                tokenLen = repeatCount(format, i, c);
                if (tokenLen > 7 || seenFraction) return false;
                std::int64_t raw = 0;
                // `f` REQUIRES its digits and `F` does not: .NET calls the same reader for both
                // and simply ignores the result for `F`, which is why the two differ by one
                // discarded return value rather than by a different reader.
                const bool read = takeDigits(input, pos, tokenLen, tokenLen, raw, digitsRead);
                if (c == 'f' && !read) return false;
                if (read) {
                    // Scale the fraction to ticks by its WIDTH, so "5" under `f` is 0.5s and
                    // "5" under `fffffff` is 5 ticks.
                    std::int64_t scale = 1;
                    for (int k = digitsRead; k < 7; ++k) scale *= 10;
                    fields.fractionTicks = raw * scale;
                }
                seenFraction = true;
            } else if (c == '\'' || c == '"') {
                const char quote = c;
                std::size_t j = i + 1;
                bool closed = false;
                while (j < format.size()) {
                    if (format[j] == quote) { closed = true; break; }
                    if (pos >= input.size() || input[pos] != format[j]) return false;
                    ++pos; ++j;
                }
                if (!closed) return false;   // .NET's SetBadQuoteFailure
                tokenLen = static_cast<int>(j - i + 1);
            } else if (c == '%') {
                // The optional-format escape. `%%` and a trailing `%` are BOTH errors in .NET,
                // and the second is the one an `i + 1 < size()` guard alone would get wrong.
                if (i + 1 >= format.size() || format[i + 1] == '%') return false;
                tokenLen = 1;                // skip the '%' and reprocess the next character
            } else if (c == '\\') {
                if (i + 1 >= format.size()) return false;
                if (pos >= input.size() || input[pos] != format[i + 1]) return false;
                ++pos;
                tokenLen = 2;
            } else {
                // THE ROW THAT SEPARATES THIS GRAMMAR FROM THE DATE/TIME ONE: an unquoted
                // literal is an ERROR, not a character to match. `"hh:mm"` is not a valid
                // TimeSpan format in .NET, and a scanner that matched it would accept formats
                // the reference rejects.
                return false;
            }
            i += static_cast<std::size_t>(tokenLen);
        }

        // The custom format must consume the ENTIRE input (.NET's `tokenizer.EOL`).
        if (pos != input.size()) return false;

        // .NET's per-COMPONENT bounds (TimeSpanParse.cs:59-62). They are not a total-range check:
        // "25" against "hh" fails rather than carrying into days.
        if (fields.days > 10675199 || fields.hours > 23 ||
            fields.minutes > 59 || fields.seconds > 59)
            return false;

        return true;
    }

} // namespace System::detail
