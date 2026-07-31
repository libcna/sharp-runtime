// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

#include "System/FormatException.hpp"
#include "System/OutOfMemoryException.hpp"

/**
 * @file
 * @brief The one composite-format grammar `String::Format` and
 *        `FormattableString::ToString` both scan with (ticket #1884, SR-AUD-015
 *        tail, CCF-012).
 *
 * Tickets #1882 and #1883 gave each engine a single left-to-right pass, which
 * closed four non-termination and undefined-behaviour defects but deliberately
 * left the accepted **grammar** alone, because changing it alters what
 * currently-succeeding calls return. #1884 adopts .NET's grammar, and this
 * header is where it lives so the two engines cannot answer differently.
 *
 * The state machine is `ValueStringBuilder.AppendFormat.cs`'s
 * `AppendFormatHelper`, transcribed: escaped braces, an unescaped `}` rejected,
 * an index limit, an alignment component that pads, and a specifier that may not
 * contain `{`. What the port does **not** adopt is recorded in
 * `docs/CompositeFormatBoundaryPlan.md` §20.3.1 — the approval covers the
 * grammar rows of §20.1 only, not culture, `ICustomFormatter`, or the
 * whitespace-inside-a-hole spellings.
 */
namespace System::detail {

/** @brief `0 <= index < IndexLimit`, exactly .NET's `AppendFormatHelper` bound. */
inline constexpr long long kCompositeIndexLimit = 1'000'000LL;
/** @brief `-WidthLimit < alignment < WidthLimit`, exactly .NET's bound. */
inline constexpr long long kCompositeWidthLimit = 1'000'000LL;

/**
 * @brief Raises .NET's `Format_InvalidStringWithOffsetAndReason`.
 * @param offset Index of the offending character, as .NET reports it.
 * @param reason One of .NET's `Format_UnexpectedClosingBrace`,
 *               `Format_UnclosedFormatItem` or `Format_ExpectedAsciiDigit`.
 */
[[noreturn]] inline void throwCompositeFormatInvalid(std::size_t offset, const char* reason) {
    throw System::FormatException(
        "Input string was not in a correct format. Failure to parse near offset " +
        std::to_string(offset) + ". " + reason);
}

/** @brief Raises .NET's `Format_IndexOutOfRange`. */
[[noreturn]] inline void throwCompositeFormatIndexOutOfRange() {
    throw System::FormatException(
        "Index (zero based) must be greater than or equal to zero and less than "
        "the size of the argument list.");
}

/**
 * @brief Appends @p count spaces, reporting an allocation failure as .NET does.
 *
 * An accepted alignment can be as wide as ten million, so this allocation can
 * genuinely fail. Letting `std::bad_alloc` or `std::length_error` escape would
 * reintroduce the defect #1882 removed — a non-`System` exception leaving a
 * `System`-shaped public API.
 */
inline void appendCompositePadding(std::string& out, long long count) {
    try {
        out.append(static_cast<std::size_t>(count), ' ');
    } catch (const std::bad_alloc&) {
        throw System::OutOfMemoryException();
    } catch (const std::length_error&) {
        throw System::OutOfMemoryException();
    }
}

/**
 * @brief Runs .NET's composite-format grammar over @p format in one pass.
 *
 * @tparam Render  Callable as `std::string(std::size_t index, std::string_view spec)`.
 *                 It is invoked only after the index has been range-checked, so a
 *                 malformed item can never be reported after part of its
 *                 substitution was published.
 * @param format   The composite format string.
 * @param argCount Number of available arguments.
 * @param render   Produces the substituted text for one item.
 * @return The formatted result.
 * @throws System::FormatException for a malformed format string or an index with
 *         no corresponding argument.
 * @throws System::OutOfMemoryException if alignment padding cannot be allocated.
 */
template <class Render>
std::string runCompositeFormat(std::string_view format, std::size_t argCount, Render&& render) {
    std::string out;
    out.reserve(format.size() + 16 * argCount);

    const std::size_t n = format.size();
    std::size_t pos = 0;

    // .NET's MoveNext: stepping past the end inside an item is "ends prematurely",
    // never a silent truncation.
    auto moveNext = [&]() -> char {
        ++pos;
        if (pos >= n) throwCompositeFormatInvalid(pos, "Format item ends prematurely.");
        return format[pos];
    };

    while (true) {
        while (pos < n && format[pos] != '{' && format[pos] != '}') out.push_back(format[pos++]);
        if (pos >= n) return out;

        // A brace must be followed by something: either a copy of itself (an
        // escape) or the body of an item.
        const char brace = format[pos];
        char ch = moveNext();
        if (ch == brace) {
            out.push_back(ch);
            ++pos;
            continue;
        }
        if (brace != '{') {
            throwCompositeFormatInvalid(
                pos, "Unexpected closing brace without a corresponding opening brace.");
        }

        long long        width       = 0;
        bool             leftJustify = false;
        std::string_view spec;

        if (ch < '0' || ch > '9') throwCompositeFormatInvalid(pos, "Expected an ASCII digit.");
        long long index = ch - '0';

        ch = moveNext();
        if (ch != '}') {
            while (ch >= '0' && ch <= '9' && index < kCompositeIndexLimit) {
                index = index * 10 + (ch - '0');
                ch = moveNext();
            }
            while (ch == ' ') ch = moveNext();

            if (ch == ',') {
                do { ch = moveNext(); } while (ch == ' ');
                if (ch == '-') {
                    leftJustify = true;
                    ch = moveNext();
                }
                if (ch < '0' || ch > '9') throwCompositeFormatInvalid(pos, "Expected an ASCII digit.");
                width = ch - '0';
                ch = moveNext();
                while (ch >= '0' && ch <= '9' && width < kCompositeWidthLimit) {
                    width = width * 10 + (ch - '0');
                    ch = moveNext();
                }
                while (ch == ' ') ch = moveNext();
            }

            if (ch != '}') {
                if (ch != ':') throwCompositeFormatInvalid(pos, "Format item ends prematurely.");
                const std::size_t start = pos + 1;
                while (true) {
                    ch = moveNext();
                    if (ch == '}') break;
                    // Braces inside an item's specifier are not supported, which is
                    // what makes "{0:{1}}" a FormatException rather than two
                    // differently-wrong readings of the same text.
                    if (ch == '{') throwCompositeFormatInvalid(pos, "Format item ends prematurely.");
                }
                spec = format.substr(start, pos - start);
            }
        }
        ++pos;  // past the closing brace

        if (index >= static_cast<long long>(argCount)) throwCompositeFormatIndexOutOfRange();

        const std::string substituted = render(static_cast<std::size_t>(index), spec);
        const long long   length      = static_cast<long long>(substituted.size());
        if (width <= length) {
            out += substituted;
        } else if (leftJustify) {
            out += substituted;
            appendCompositePadding(out, width - length);
        } else {
            appendCompositePadding(out, width - length);
            out += substituted;
        }
    }
}

} // namespace System::detail
