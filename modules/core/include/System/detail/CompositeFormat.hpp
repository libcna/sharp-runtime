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
 *
 * ---
 *
 * **Ticket #2020 (SR-AUD-298 grammar half, CCF-012) turned this into a scanner two
 * callers share, and found the finding's second claim to be false.**
 *
 * CCF-012's warning is that *altering just one API preserves divergent brace rules*, and
 * `System::Text::CompositeFormat::Parse` was a third hand-written grammar. It now scans
 * with `scanCompositeFormat` below, so there is exactly one definition of "what is a
 * format item" in this repository.
 *
 * But the two engines must **not** become byte-identical, because in .NET they are not.
 * `CompositeFormat.TryParseLiterals` says of itself *"This parsing logic is copied from
 * string.Format"* (`CompositeFormat.cs:113-116`), and it is — with two characters removed.
 * `AppendFormatHelper` writes `while (char.IsAsciiDigit(ch) && index < IndexLimit)` and
 * `… && width < WidthLimit` (`ValueStringBuilder.AppendFormat.cs:99,140`); `TryParseLiterals`
 * writes `while (char.IsAsciiDigit(ch))` for both (`CompositeFormat.cs:201,258`). Nothing
 * else differs.
 *
 * So the ticket's premise that `Parse("{1500000}")` should be rejected "because .NET's
 * `AppendFormatHelper` index limit is 1,000,000" is measurably **wrong**: .NET's `Parse`
 * has no index limit, and this port's answer of `1500001` was already right. Adopting the
 * formatter's limits wholesale — the plan's proposal — would have introduced a *new*
 * divergence while claiming to remove one. The single axis is therefore a parameter
 * (@ref CompositeDigitPolicy), not a duplicated function.
 */
namespace System::detail {

/** @brief `0 <= index < IndexLimit`, exactly .NET's `AppendFormatHelper` bound. */
inline constexpr long long kCompositeIndexLimit = 1'000'000LL;
/** @brief `-WidthLimit < alignment < WidthLimit`, exactly .NET's bound. */
inline constexpr long long kCompositeWidthLimit = 1'000'000LL;

/**
 * @brief The only axis on which .NET's two composite-format grammars differ.
 *
 * @see The file comment. `AppendFormatHelper` stops consuming index and alignment digits at
 *      one million; `CompositeFormat.TryParseLiterals` does not stop at all.
 */
enum class CompositeDigitPolicy {
    /** `String::Format` / `FormattableString::ToString` — .NET's `AppendFormatHelper`. */
    BoundedByFormatLimits,
    /** `System::Text::CompositeFormat::Parse` — .NET's `TryParseLiterals`. */
    UnboundedLikeParse,
};

/**
 * @brief The largest index whose `+ 1` is representable as `intcs`.
 *
 * **This bound is a deliberate, measured deviation from .NET 11, and the one place where
 * #2020 does not follow the reference.** `TryParseLiterals` accumulates the index with
 * `index = index * 10 + ch - '0'` into an `int` in an unchecked context, so
 * `Parse("{2147483648}")` *wraps* to a negative `ArgIndex`; the constructor then counts it
 * as neither a literal nor a hole (`CompositeFormat.cs:48-56`), and one value earlier
 * `Parse("{2147483647}")` computes `Math.Max(0, int.MaxValue + 1)` and reports
 * `MinimumArgumentCount == 0` for a format string that needs two billion arguments.
 *
 * C++ signed overflow is undefined rather than wrapping, so reproducing that is not merely
 * undesirable but unavailable. Ticket #2010 chose the exception, and #2020 keeps it: a
 * `FormatException` naming the problem beats a silently wrong count, which is what
 * `CLAUDE.md`'s parity philosophy asks for where the two conflict. Every index .NET answers
 * *correctly* is answered identically.
 */
inline constexpr long long kCompositeMaxRepresentableIndex = 2'147'483'646LL;

/**
 * @brief Where alignment accumulation saturates in @ref CompositeDigitPolicy::UnboundedLikeParse.
 *
 * .NET wraps here too, and the alignment is not observable through this port's
 * `CompositeFormat` — it exposes `Format` and `MinimumArgumentCount` and nothing else — so
 * the *value* cannot be seen. What must match is which texts are **accepted**, and
 * saturating consumes exactly the digit runs .NET consumes without undefined behaviour.
 */
inline constexpr long long kCompositeWidthSaturation = 1'000'000'000'000'000LL;

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
 * @brief Scans .NET's composite-format grammar over @p format in one left-to-right pass.
 *
 * The single definition of "what is a format item" in this repository. Ticket #2020
 * extracted it so `String::Format`, `FormattableString::ToString` and
 * `System::Text::CompositeFormat::Parse` cannot answer differently — the divergence
 * CCF-012 warns about, and which a third hand-written parser had already produced.
 *
 * Nothing here renders, pads or allocates a result: a caller that only needs to *validate*
 * a format string (`CompositeFormat::Parse`) has no argument list to render from, which is
 * why adopting the formatter directly was never possible.
 *
 * @tparam OnLiteral Callable as `void(std::string_view run)`. Receives each run of literal
 *                   text, and separately the single character an escaped `{{` or `}}`
 *                   resolves to. Runs are handed over unescaped exactly as .NET's
 *                   `ValueStringBuilder` receives them.
 * @tparam OnItem    Callable as
 *                   `void(long long index, long long width, bool leftJustify, std::string_view spec)`.
 *                   Called once per format item, after that item is fully parsed and before
 *                   any later text is scanned, so a caller that throws on the index reports
 *                   it at .NET's position in the stream.
 * @param format     The composite format string.
 * @param policy     Which of .NET's two digit rules to apply; see @ref CompositeDigitPolicy.
 * @throws System::FormatException for a malformed format string, and — under
 *         @ref CompositeDigitPolicy::UnboundedLikeParse only — for an index above
 *         @ref kCompositeMaxRepresentableIndex.
 */
template <class OnLiteral, class OnItem>
void scanCompositeFormat(std::string_view format, CompositeDigitPolicy policy,
                         OnLiteral&& onLiteral, OnItem&& onItem) {
    const std::size_t n       = format.size();
    const bool        bounded = policy == CompositeDigitPolicy::BoundedByFormatLimits;
    std::size_t       pos     = 0;

    // .NET's MoveNext: stepping past the end inside an item is "ends prematurely",
    // never a silent truncation.
    auto moveNext = [&]() -> char {
        ++pos;
        if (pos >= n) throwCompositeFormatInvalid(pos, "Format item ends prematurely.");
        return format[pos];
    };

    while (true) {
        const std::size_t runStart = pos;
        while (pos < n && format[pos] != '{' && format[pos] != '}') ++pos;
        if (pos > runStart) onLiteral(format.substr(runStart, pos - runStart));
        if (pos >= n) return;

        // A brace must be followed by something: either a copy of itself (an
        // escape) or the body of an item.
        const char brace = format[pos];
        char ch = moveNext();
        if (ch == brace) {
            onLiteral(format.substr(pos, 1));
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
            while (ch >= '0' && ch <= '9' && (!bounded || index < kCompositeIndexLimit)) {
                index = index * 10 + (ch - '0');
                // Unreachable under BoundedByFormatLimits, where the loop condition already
                // caps the value at 9,999,999. Under UnboundedLikeParse it is the guard
                // kCompositeMaxRepresentableIndex documents.
                if (index > kCompositeMaxRepresentableIndex) {
                    throwCompositeFormatInvalid(
                        pos, "Format item's argument index is too large to be represented.");
                }
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
                while (ch >= '0' && ch <= '9' && (!bounded || width < kCompositeWidthLimit)) {
                    // Saturates rather than overflows; see kCompositeWidthSaturation. The
                    // comparison is always true under BoundedByFormatLimits.
                    if (width <= kCompositeWidthSaturation) width = width * 10 + (ch - '0');
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

        onItem(index, width, leftJustify, spec);
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
 *
 * @note Since #2020 this is a thin adaptor over @ref scanCompositeFormat. The grammar it
 *       applies is unchanged — `CompositeDigitPolicy::BoundedByFormatLimits` is exactly the
 *       state machine that lived here before.
 */
template <class Render>
std::string runCompositeFormat(std::string_view format, std::size_t argCount, Render&& render) {
    std::string out;
    out.reserve(format.size() + 16 * argCount);

    scanCompositeFormat(
        format, CompositeDigitPolicy::BoundedByFormatLimits,
        [&](std::string_view run) { out += run; },
        [&](long long index, long long width, bool leftJustify, std::string_view spec) {
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
        });

    return out;
}

} // namespace System::detail
