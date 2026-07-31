// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <string>
#include <string_view>

/**
 * @file
 * @brief The two pieces of .NET's default floating-point number grammar that
 *        @c std::from_chars cannot express (ticket #1865, SR-AUD-033 parse tail).
 *
 * `Single::Parse`/`Double::Parse` mirror .NET's
 * `NumberStyles.Float | NumberStyles.AllowThousands`
 * (`Double.cs:388`, `Single.cs`). Two parts of that grammar have no
 * `std::from_chars` equivalent:
 *
 *  1. **Group separators.** `std::from_chars` has no grouping mode at all, so a
 *     `,` must be validated and removed before the conversion.
 *  2. **Out-of-range magnitudes.** `std::from_chars` reports
 *     `std::errc::result_out_of_range` and — measured on libstdc++ 2026-07-31,
 *     `build-probe/1865_prefix_plain.log` cases B01-B08 — leaves the output
 *     **unmodified**, for overflow *and* underflow alike. .NET treats neither as
 *     a format error: a magnitude past the type's range saturates to
 *     ±Infinity, and one below it collapses to ±0.
 *
 * Both helpers live here rather than being duplicated in `Single.hpp` and
 * `Double.hpp`, so the two types cannot drift apart. Both are `noexcept` and
 * neither allocates unless the input actually contains a group separator.
 */
namespace System::detail {

/**
 * @brief Validates and removes invariant-culture `,` group separators, mirroring
 *        .NET's `NumberStyles.AllowThousands` scanner rule.
 *
 * .NET's scanner (`Number.Parsing.Common.cs`, `TryParseNumber`) accepts a group
 * separator only when it has already seen a digit and has **not** yet seen the
 * decimal separator; anywhere else the scan simply stops, and the parse then
 * fails because the whole string was not consumed. It deliberately does **not**
 * check group *sizes* — group sizes are a formatting concept — so `"1,2,3"`
 * parses as `123` and `"1,"` as `1`, exactly as it does in .NET.
 *
 * @param in      The whitespace-trimmed candidate text.
 * @param scratch Storage for the separator-free copy. Written **only** when
 *                @p in actually contains a `,`; untouched otherwise.
 * @param out     Set to the text the numeric conversion should see — either
 *                @p in itself or a view over @p scratch.
 * @return @c false if a `,` appears where .NET's scanner would stop.
 */
[[nodiscard]] inline bool tryStripGroupSeparators(std::string_view in,
                                                  std::string& scratch,
                                                  std::string_view& out) noexcept {
    if (in.find(',') == std::string_view::npos) {
        out = in;
        return true;
    }
    bool seenDigit = false, seenDecimalPoint = false, seenExponent = false;
    std::size_t kept = 0;
    // The copy can only shrink, so reserving the source length is enough and the
    // single allocation is the only one this path performs.
    try {
        scratch.assign(in.size(), '\0');
    } catch (...) {
        return false;  // keeps the caller's noexcept contract honest
    }
    for (char c : in) {
        if (c == ',') {
            if (!seenDigit || seenDecimalPoint || seenExponent) return false;
            continue;  // accepted and dropped
        }
        if (c >= '0' && c <= '9') seenDigit = true;
        else if (c == '.') seenDecimalPoint = true;
        else if (c == 'e' || c == 'E') seenExponent = true;
        scratch[kept++] = c;
    }
    scratch.resize(kept);
    out = scratch;
    return true;
}

/**
 * @brief Decides whether a magnitude `std::from_chars` rejected as out of range
 *        was too **large** (overflow) rather than too **small** (underflow).
 *
 * `std::from_chars` uses one `errc::result_out_of_range` for both directions and
 * does not write the output, so the direction has to be recovered from the text.
 * The test is exact rather than approximate: the value is written in normalized
 * scientific form `d.ddd × 10^E`, and every magnitude with `E >= 0` is at least
 * 1 — which is always representable — so such a value can only have been
 * rejected for being too large. Symmetrically every `E < 0` magnitude is below 1
 * and can only have been rejected for being too small.
 *
 * @param numeric The separator-free numeric text, as handed to `from_chars`.
 * @return @c true for an overflow, @c false for an underflow or an all-zero
 *         significand (which is representable and cannot be out of range).
 */
[[nodiscard]] inline bool outOfRangeIsOverflow(std::string_view numeric) noexcept {
    std::size_t i = 0;
    if (i < numeric.size() && (numeric[i] == '+' || numeric[i] == '-')) ++i;

    const std::size_t intBegin = i;
    while (i < numeric.size() && numeric[i] >= '0' && numeric[i] <= '9') ++i;
    const std::size_t intEnd = i;

    std::size_t fracBegin = i, fracEnd = i;
    if (i < numeric.size() && numeric[i] == '.') {
        fracBegin = ++i;
        while (i < numeric.size() && numeric[i] >= '0' && numeric[i] <= '9') ++i;
        fracEnd = i;
    }

    long long exponent = 0;
    if (i < numeric.size() && (numeric[i] == 'e' || numeric[i] == 'E')) {
        ++i;
        bool negativeExponent = false;
        if (i < numeric.size() && (numeric[i] == '+' || numeric[i] == '-')) {
            negativeExponent = numeric[i] == '-';
            ++i;
        }
        while (i < numeric.size() && numeric[i] >= '0' && numeric[i] <= '9') {
            // Saturate rather than overflow: ".NET" itself clamps an exponent
            // past 9 digits (Number.Parsing.Common.cs pins it at int.MaxValue),
            // and 10^9 is already far outside every floating type's range.
            if (exponent < 1000000000LL) exponent = exponent * 10 + (numeric[i] - '0');
            ++i;
        }
        if (negativeExponent) exponent = -exponent;
    }

    std::size_t k = intBegin;
    while (k < intEnd && numeric[k] == '0') ++k;
    long long normalized;
    if (k < intEnd) {
        normalized = static_cast<long long>(intEnd - k) - 1;
    } else {
        std::size_t j = fracBegin;
        while (j < fracEnd && numeric[j] == '0') ++j;
        if (j >= fracEnd) return false;  // no significant digit at all
        normalized = -static_cast<long long>(j - fracBegin + 1);
    }
    // |normalized| is bounded by the input length and |exponent| by 10^10, so
    // the sum cannot overflow long long.
    return normalized + exponent >= 0;
}

} // namespace System::detail
