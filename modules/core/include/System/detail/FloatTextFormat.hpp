// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <charconv>
#include <cstddef>
#include <string>
#include <system_error>

/**
 * @file
 * @brief The three things `std::ostringstream` cannot do that .NET's `E`, `N`
 *        and `G` float formats require (ticket #1863, SR-AUD-033 format slice,
 *        CCF-007 item CCF7-5).
 *
 * `Single::ToString(value, format)` and `Double::ToString(value, format)` are an
 * `std::ostringstream` back end. libstdc++ emits a **two**-digit exponent where
 * .NET emits at least three; the stream has no grouping mode at all, so `N` was
 * formatted identically to `F`; and `setprecision` gives a fixed digit count
 * rather than the shortest round-trippable one. These helpers post-process the
 * stream's output for `E` and `N`, and replace it outright for `G`.
 *
 * Shared by both types so their emitted text cannot drift apart.
 */
namespace System::detail {

/**
 * @brief Pads a scientific-notation exponent to .NET's minimum of three digits.
 *
 * .NET's `E` standard format always writes a sign and **at least three**
 * exponent digits (`1.25E+000`); libstdc++'s `std::scientific` writes the C
 * minimum of two (`1.25E+00`). Only the exponent field is touched, so the sign,
 * the mantissa and the case of the exponent letter are exactly what the stream
 * produced.
 *
 * @param streamText Text from `std::scientific`, e.g. `"1.25E+00"`.
 * @return The same text with the exponent zero-padded to at least three digits.
 */
[[nodiscard]] inline std::string padExponentToThreeDigits(std::string streamText) {
    const std::size_t marker = streamText.find_last_of("eE");
    if (marker == std::string::npos) return streamText;
    std::size_t digitsBegin = marker + 1;
    if (digitsBegin < streamText.size() &&
        (streamText[digitsBegin] == '+' || streamText[digitsBegin] == '-'))
        ++digitsBegin;
    std::size_t digitCount = 0;
    while (digitsBegin + digitCount < streamText.size() &&
           streamText[digitsBegin + digitCount] >= '0' &&
           streamText[digitsBegin + digitCount] <= '9')
        ++digitCount;
    if (digitCount == 0 || digitCount >= 3) return streamText;
    streamText.insert(digitsBegin, 3 - digitCount, '0');
    return streamText;
}

/**
 * @brief Inserts invariant-culture group separators into the integer part.
 *
 * .NET's `N` standard format uses `NumberFormatInfo.InvariantInfo`'s
 * `NumberGroupSizes = { 3 }` and `NumberGroupSeparator = ","`, so `1234.5`
 * formatted as `N2` is `"1,234.50"`. The stream cannot do this at all, which is
 * why `N` used to be formatted identically to `F`. The fractional part, the sign
 * and the decimal point are untouched.
 *
 * @param streamText Text from `std::fixed`, e.g. `"-1234567.89"`.
 * @return The same text with `,` every three integer digits, e.g. `"-1,234,567.89"`.
 */
[[nodiscard]] inline std::string insertGroupSeparators(const std::string& streamText) {
    std::size_t begin = 0;
    if (begin < streamText.size() && (streamText[begin] == '-' || streamText[begin] == '+')) ++begin;
    std::size_t end = begin;
    while (end < streamText.size() && streamText[end] >= '0' && streamText[end] <= '9') ++end;
    const std::size_t digitCount = end - begin;
    if (digitCount <= 3) return streamText;

    std::string grouped;
    grouped.reserve(streamText.size() + digitCount / 3);
    grouped.append(streamText, 0, begin);
    for (std::size_t i = 0; i < digitCount; ++i) {
        if (i != 0 && (digitCount - i) % 3 == 0) grouped.push_back(',');
        grouped.push_back(streamText[begin + i]);
    }
    grouped.append(streamText, end, std::string::npos);
    return grouped;
}

/**
 * @brief Formats @p value to exactly @p significantDigits significant digits.
 *
 * .NET's `G<n>` asks for a round-trip-capable representation at a given
 * precision — `G17` for `double` and `G9` for `float` are the widths that
 * round-trip every finite value. `std::to_chars`' general form is the
 * shortest-correctly-rounded algorithm at that precision;
 * `std::ostringstream`'s `setprecision` is not specified to be, which is what
 * §19.2 of `docs/FloatingValueFidelityPlan.md` recorded as the gap.
 *
 * @tparam T `float` or `double`.
 * @param value             The value to format.
 * @param significantDigits The `G` precision; must be positive.
 * @return The formatted text, or an empty string if the buffer was too small
 *         (which the caller treats as "fall back to the stream").
 */
template <class T>
[[nodiscard]] inline std::string generalWithPrecision(T value, int significantDigits) {
    std::array<char, 512> buffer{};
    const auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                         std::chars_format::general, significantDigits);
    if (ec != std::errc{}) return {};
    return std::string(buffer.data(), ptr);
}

} // namespace System::detail
