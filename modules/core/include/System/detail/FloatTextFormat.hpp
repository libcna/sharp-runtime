// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
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

/**
 * @brief Whether @p format is a .NET **standard** numeric format string.
 *
 * .NET reads a format as standard only when it is a single alphabetic character
 * optionally followed by a precision of decimal digits (`"F2"`, `"G"`, `"E3"`).
 * Everything else -- `"0.000"`, `"#,##0.0"`, `"00"` -- is a **custom** numeric
 * format string, a completely separate grammar.
 *
 * @param format The format string; must not be empty.
 * @return True when @p format has the standard shape.
 */
[[nodiscard]] inline bool isStandardNumericFormat(const std::string& format) {
    if (format.empty()) return false;
    const unsigned char first = static_cast<unsigned char>(format[0]);
    if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z'))) return false;
    for (std::size_t i = 1; i < format.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(format[i]);
        if (c < '0' || c > '9') return false;
    }
    return true;
}

/**
 * @brief The subset of .NET's custom numeric format grammar this build implements.
 *
 * Implemented: the digit placeholders `0` (always emitted) and `#` (emitted only
 * when significant), the decimal point `.`, and `,` used as a group separator
 * between integer placeholders. Any other character is copied through as a
 * literal, which is what .NET does with an unrecognised character.
 *
 * Not implemented, and refused rather than silently mis-emitted: the `;` section
 * separator, the `%` and `‰` scaling specifiers, the custom `E0` exponent forms,
 * and `\` / quote escaping.
 */
struct CustomNumericFormat {
    std::size_t minimumIntegerDigits = 0;  ///< Count of `0` placeholders left of the point.
    std::size_t maximumDecimals = 0;       ///< Count of placeholders right of the point.
    std::size_t minimumDecimals = 0;       ///< Count of `0` placeholders right of the point.
    bool hasDecimalPoint = false;          ///< Whether the format contains a `.`.
    bool groupSeparators = false;          ///< Whether a `,` sits between integer placeholders.
};

/**
 * @brief Parses a custom numeric format string.
 *
 * @param format The custom format, e.g. `"0.000"`.
 * @return The parsed shape.
 * @throws System::FormatException via @p onUnsupported for a construct this build
 *         does not implement.
 */
template <class OnUnsupported>
[[nodiscard]] inline CustomNumericFormat parseCustomNumericFormat(const std::string& format,
                                                                 OnUnsupported onUnsupported) {
    CustomNumericFormat shape;
    bool afterPoint = false;
    bool sawIntegerPlaceholder = false;
    for (std::size_t i = 0; i < format.size(); ++i) {
        const char c = format[i];
        if (c == ';' || c == '%' || c == '\\' || c == '\'' || c == '"' ||
            ((c == 'E' || c == 'e') && i + 1 < format.size() &&
             (format[i + 1] == '0' || format[i + 1] == '+' || format[i + 1] == '-'))) {
            onUnsupported();
        }
        if (c == '.') {
            // Only the first point is the decimal separator; later ones are literals.
            if (!shape.hasDecimalPoint) shape.hasDecimalPoint = true;
            afterPoint = true;
            continue;
        }
        if (c == ',') {
            // A comma only groups when it sits between integer digit placeholders.
            if (!afterPoint && sawIntegerPlaceholder) shape.groupSeparators = true;
            continue;
        }
        if (c == '0' || c == '#') {
            if (afterPoint) {
                ++shape.maximumDecimals;
                if (c == '0') shape.minimumDecimals = shape.maximumDecimals;
            } else {
                sawIntegerPlaceholder = true;
                if (c == '0') ++shape.minimumIntegerDigits;
            }
        }
    }
    return shape;
}

/**
 * @brief Splits a decimal text into its sign, integer digits and fraction digits.
 *
 * Accepts the shortest round-trippable text `to_chars` produces, including the
 * exponential forms (`1e-07`), which are expanded so the caller only ever sees
 * plain digit strings.
 *
 * @param text A decimal or exponential number, e.g. `"-0.5"` or `"1e-07"`.
 * @param negative Set to true when @p text is negative.
 * @param integerDigits Receives the integer digits, without leading zeros.
 * @param fractionDigits Receives the fraction digits.
 */
inline void splitDecimalText(const std::string& text, bool& negative,
                             std::string& integerDigits, std::string& fractionDigits) {
    negative = false;
    std::size_t i = 0;
    if (i < text.size() && (text[i] == '-' || text[i] == '+')) {
        negative = text[i] == '-';
        ++i;
    }
    std::string digits;
    int pointPosition = -1;
    int exponent = 0;
    for (; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '.') { pointPosition = static_cast<int>(digits.size()); continue; }
        if (c == 'e' || c == 'E') { exponent = std::stoi(text.substr(i + 1)); break; }
        digits.push_back(c);
    }
    if (pointPosition < 0) pointPosition = static_cast<int>(digits.size());
    pointPosition += exponent;
    while (pointPosition < 0) { digits.insert(digits.begin(), '0'); ++pointPosition; }
    while (static_cast<std::size_t>(pointPosition) > digits.size()) digits.push_back('0');
    integerDigits = digits.substr(0, static_cast<std::size_t>(pointPosition));
    fractionDigits = digits.substr(static_cast<std::size_t>(pointPosition));
    std::size_t firstSignificant = integerDigits.find_first_not_of('0');
    integerDigits = firstSignificant == std::string::npos
                        ? std::string()
                        : integerDigits.substr(firstSignificant);
}

/**
 * @brief Rounds decimal digit strings at @p decimals places, half away from zero.
 *
 * .NET's number formatting rounds midpoints away from zero, which neither
 * `std::fixed` nor `std::to_chars` can be asked for. Doing it on the digits
 * avoids the question entirely.
 *
 * @param integerDigits Integer digits; rounded in place, may gain a digit.
 * @param fractionDigits Fraction digits; truncated in place to @p decimals.
 * @param decimals How many fraction digits to keep.
 */
inline void roundDecimalDigits(std::string& integerDigits, std::string& fractionDigits,
                               std::size_t decimals) {
    if (fractionDigits.size() <= decimals) return;
    const bool roundUp = fractionDigits[decimals] >= '5';
    fractionDigits.resize(decimals);
    if (!roundUp) return;
    for (std::size_t i = fractionDigits.size(); i-- > 0;) {
        if (fractionDigits[i] != '9') { ++fractionDigits[i]; return; }
        fractionDigits[i] = '0';
    }
    for (std::size_t i = integerDigits.size(); i-- > 0;) {
        if (integerDigits[i] != '9') { ++integerDigits[i]; return; }
        integerDigits[i] = '0';
    }
    integerDigits.insert(integerDigits.begin(), '1');
}

/**
 * @brief Emits a value's digits through a custom numeric format string.
 *
 * Walks @p format so that every character which is not a digit placeholder is copied
 * through as a literal, which is what .NET does -- `(1f).ToString("Fx")` is `"Fx"`,
 * measured against the reference implementation rather than assumed. Integer digits are
 * consumed right to left, so any digits the format has no placeholder for are emitted at
 * the leftmost placeholder; a format with no integer placeholder at all emits none of
 * them.
 *
 * @param negative Whether the value is negative.
 * @param integerDigits The integer digits, without leading zeros.
 * @param fractionDigits The fraction digits, already rounded to the format's width.
 * @param format The custom format string.
 * @param shape The same format, already parsed.
 * @return The formatted text.
 */
[[nodiscard]] inline std::string emitCustomNumeric(bool negative,
                                                  const std::string& integerDigits,
                                                  std::string fractionDigits,
                                                  const std::string& format,
                                                  const CustomNumericFormat& shape) {
    const std::size_t pointInFormat = format.find('.');
    const std::string integerFormat =
        pointInFormat == std::string::npos ? format : format.substr(0, pointInFormat);
    const std::string fractionFormat =
        pointInFormat == std::string::npos ? std::string() : format.substr(pointInFormat + 1);

    // A trailing `#` run is dropped only as far as the `0` placeholders allow.
    while (fractionDigits.size() > shape.minimumDecimals && !fractionDigits.empty() &&
           fractionDigits.back() == '0')
        fractionDigits.pop_back();

    std::string integerText;
    std::size_t remaining = integerDigits.size();
    bool emittedAnyIntegerDigit = false;
    std::size_t emittedInGroup = 0;
    for (std::size_t i = integerFormat.size(); i-- > 0;) {
        const char c = integerFormat[i];
        if (c == '0' || c == '#') {
            if (shape.groupSeparators && emittedInGroup == 3) {
                integerText.push_back(',');
                emittedInGroup = 0;
            }
            if (remaining > 0) {
                integerText.push_back(integerDigits[--remaining]);
                emittedAnyIntegerDigit = true;
                ++emittedInGroup;
            } else if (c == '0') {
                integerText.push_back('0');
                emittedAnyIntegerDigit = true;
                ++emittedInGroup;
            }
            // The leftmost placeholder takes every digit the format had no room for.
            const bool leftmost = integerFormat.find_first_of("0#") == i;
            if (leftmost) {
                while (remaining > 0) {
                    if (shape.groupSeparators && emittedInGroup == 3) {
                        integerText.push_back(',');
                        emittedInGroup = 0;
                    }
                    integerText.push_back(integerDigits[--remaining]);
                    emittedAnyIntegerDigit = true;
                    ++emittedInGroup;
                }
            }
            continue;
        }
        // A comma is the group separator, already accounted for; anything else is a literal.
        if (c == ',') continue;
        integerText.push_back(c);
    }
    (void)emittedAnyIntegerDigit;
    std::reverse(integerText.begin(), integerText.end());

    std::string fractionText;
    std::size_t taken = 0;
    for (const char c : fractionFormat) {
        if (c == '0' || c == '#') {
            if (taken < fractionDigits.size()) {
                fractionText.push_back(fractionDigits[taken++]);
            } else if (c == '0') {
                fractionText.push_back('0');
            }
            continue;
        }
        if (c == ',') continue;
        fractionText.push_back(c);
    }

    // .NET drops the decimal point when nothing was emitted after it: "0.##" of 1 is "1".
    const bool anyFractionDigit =
        fractionText.find_first_of("0123456789") != std::string::npos;

    std::string text;
    // A value that rounded away to nothing is not signed: (-0.4f).ToString("0") is "0".
    const bool anySignificant = integerDigits.find_first_not_of('0') != std::string::npos ||
                                fractionDigits.find_first_not_of('0') != std::string::npos;
    if (negative && anySignificant) text.push_back('-');
    text += integerText;
    if (shape.hasDecimalPoint && anyFractionDigit) text.push_back('.');
    text += fractionText;
    return text;
}

} // namespace System::detail
