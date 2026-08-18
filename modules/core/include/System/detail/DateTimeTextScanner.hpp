// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <string>
#include <string_view>

/**
 * @file
 * @brief The full-consumption cursor the four date/time parsers scan with
 *        (ticket #1879, SR-AUD-007b / SR-AUD-009 / SR-AUD-061, CCF-002 class D).
 *
 * `DateTime`, `DateTimeOffset`, `TimeOnly` and `DateOnly` used to verify a few
 * separator *positions* and then run a single `std::sscanf` **prefix**
 * conversion, never asserting that the conversion consumed the whole string.
 * That accepted `"2024-06-15junk"` as a date and turned `"2024-06-15 10:xx:00"`
 * into **midnight** — wrong answers a caller cannot detect. .NET's
 * `DateTimeParse.cs` is a state-machine lexer that fails on any unconsumed
 * token; this cursor is the small piece of that contract the port needs.
 *
 * It also removes `std::sscanf`'s `%d` leniencies, which are not part of any
 * documented grammar: `%d` skips leading whitespace and accepts an explicit
 * sign, so `" 024-06-15"`, `"+024-06-15"` and `"2024-06-15 +1:20:30"` all
 * parsed. And it removes `%d`'s formally undefined behaviour on a numeral
 * outside `int` by never reading more digits than fit.
 *
 * Deliberately **not** a general lexer: the port implements a narrow ISO-8601
 * subset by prior decision (`DateTime.hpp`, `TimeOnly.hpp`). Ticket **#1929**
 * (decided 2026-08-18) widened four respects of that subset to .NET's grammar --
 * see `takeDateTimeParts` and `takeUtcOffsetMinutes` below -- but the subset is
 * still a subset: no month names, no culture patterns, no two-digit year, and no
 * `ParseExact` (`docs/DateTimeValidationBoundaryPlan.md` §16.4).
 */
namespace System::detail {

/** @brief True for the invariant ASCII whitespace accepted at parser boundaries. */
[[nodiscard]] constexpr bool isDateTimeTextWhitespace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/**
 * @brief Removes leading and trailing invariant ASCII whitespace without allocating.
 *
 * Ticket #1929 row 5 deliberately applies this only at the outside boundary. Whitespace
 * inside a numeric field or next to a separator remains grammar, and remains rejected.
 */
[[nodiscard]] constexpr std::string_view trimDateTimeText(std::string_view text) noexcept {
    while (!text.empty() && isDateTimeTextWhitespace(text.front())) text.remove_prefix(1);
    while (!text.empty() && isDateTimeTextWhitespace(text.back())) text.remove_suffix(1);
    return text;
}

/** @brief A forward-only cursor over a candidate date/time string. */
class DateTimeTextScanner {
public:
    /** @brief Begins scanning @p text, which must outlive the scanner. */
    explicit DateTimeTextScanner(std::string_view text) noexcept : text_(text) {}

    /** @brief True when every character has been consumed. */
    [[nodiscard]] bool atEnd() const noexcept { return index_ >= text_.size(); }

    /** @brief Consumes @p c if it is the next character; otherwise consumes nothing. */
    bool take(char c) noexcept {
        if (index_ < text_.size() && text_[index_] == c) {
            ++index_;
            return true;
        }
        return false;
    }

    /**
     * @brief Consumes between @p minDigits and @p maxDigits ASCII digits.
     *
     * Digits only — no sign, no whitespace, no locale involvement, which is the
     * whole point of not using `std::sscanf`. Consumes nothing and returns
     * @c false unless at least @p minDigits digits are available; stops after
     * @p maxDigits so the accumulated value cannot overflow @c int (callers pass
     * at most 7).
     *
     * @param minDigits  Fewest digits that make the field well-formed.
     * @param maxDigits  Most digits the field may hold (<= 9).
     * @param value      Receives the parsed value on success; untouched on failure.
     * @param digitCount Optionally receives how many digits were consumed, which
     *                   a fractional-second field needs in order to scale.
     */
    bool takeDigits(int minDigits, int maxDigits, int& value,
                    int* digitCount = nullptr) noexcept {
        const std::size_t start = index_;
        int accumulated = 0, count = 0;
        while (index_ < text_.size() && count < maxDigits &&
               text_[index_] >= '0' && text_[index_] <= '9') {
            accumulated = accumulated * 10 + (text_[index_] - '0');
            ++index_;
            ++count;
        }
        if (count < minDigits) {
            index_ = start;  // a partial field consumes nothing
            return false;
        }
        value = accumulated;
        if (digitCount != nullptr) *digitCount = count;
        return true;
    }

private:
    std::string_view text_;
    std::size_t      index_ = 0;
};

/**
 * @brief The calendar and clock fields one date/time string carries.
 *
 * `hasTime` distinguishes a bare date from one whose clock fields are all zero,
 * which the parsers do not need but a caller reading this struct might.
 */
struct DateTimeParts {
    int  year          = 0;
    int  month         = 0;
    int  day           = 0;
    int  hour          = 0;
    int  minute        = 0;
    int  second        = 0;
    int  fractionTicks = 0;
    bool hasTime       = false;
};

/**
 * @brief Consumes this port's ISO-8601 date-and-optional-time prefix.
 *
 * @verbatim
 *     yyyy '-' M{1,2} '-' d{1,2}
 *        [ (' '|'T') H{1,2} ':' m{1,2} ':' s{1,2} [ '.' f{1,7} ] ]
 * @endverbatim
 *
 * Ticket #1929 rows 1-2 (decided 2026-08-18) widened the **month and day** to one
 * or two digits, matching .NET: its lexer classifies a run of one or two digits as
 * a `NumberToken` and three or more as a `YearNumberToken`
 * (`Globalization/DateTimeParse.cs:5593-5605`), so `"2024-6-15"` reaches the same
 * year-month-day terminal state that `"2024-06-15"` does. The **year** stays
 * exactly four digits: that is this port's ISO subset, and a two-digit year would
 * pull in `Calendar.ToFourDigitYear`'s culture-dependent century window.
 *
 * The fraction is read at 100-ns precision because `DateTime` is tick-based;
 * `.1234567` keeps all seven digits, and an eighth is rejected rather than read as
 * a prefix.
 *
 * Defined once here because three doors share the grammar. When they held their
 * own copies they drifted: `DateTimeOffset` located its offset by scanning for a
 * sign from **character 10**, which a one-digit month or day makes wrong.
 *
 * @param scanner Cursor to read from; consumes nothing on failure only insofar as
 *                each field is individually atomic — a caller that needs to retry
 *                should construct a fresh scanner.
 * @param parts   Receives the fields that were present.
 * @return @c true when the whole prefix is well-formed.
 */
[[nodiscard]] inline bool takeDateTimeParts(DateTimeTextScanner& scanner,
                                            DateTimeParts&       parts) noexcept {
    if (!scanner.takeDigits(4, 4, parts.year) || !scanner.take('-') ||
        !scanner.takeDigits(1, 2, parts.month) || !scanner.take('-') ||
        !scanner.takeDigits(1, 2, parts.day))
        return false;

    if (scanner.take(' ') || scanner.take('T')) {
        if (!scanner.takeDigits(1, 2, parts.hour) || !scanner.take(':') ||
            !scanner.takeDigits(1, 2, parts.minute) || !scanner.take(':') ||
            !scanner.takeDigits(1, 2, parts.second))
            return false;
        parts.hasTime = true;
        if (scanner.take('.')) {
            int digits = 0;
            if (!scanner.takeDigits(1, 7, parts.fractionTicks, &digits)) return false;
            while (digits < 7) { parts.fractionTicks *= 10; ++digits; }
        }
    }
    return true;
}

/**
 * @brief Consumes a signed UTC offset, transcribing .NET's `ParseTimeZone`.
 *
 * @verbatim
 *     ('+'|'-') ( d{1,2} [ ':' d{1,2} ] | d{3,4} )
 * @endverbatim
 *
 * The three-or-four-digit run is split as `value / 100` hours and `value % 100`
 * minutes, so `"+800"` and `"+0800"` both mean eight hours
 * (`Globalization/DateTimeParse.cs:530-556`). The minute field is rejected at 60
 * and above, which is .NET's only check here
 * (`DateTimeParse.cs:565-568`); the +/-14h bound belongs to the caller, because
 * .NET applies it later and only where an offset is actually stored
 * (`DateTimeParse.cs:2777,2875`).
 *
 * Ticket #1929 row 3 (decided 2026-08-18). Before it, all three doors demanded
 * exactly `+HH:MM`, so `"+2:5"`, `"+8"` and `"+0800"` were rejected -- and the
 * first of those had been read as 2h05m, the value .NET gives it, until #1879
 * narrowed the grammar.
 *
 * @param scanner       Cursor positioned at the sign character.
 * @param signedMinutes Receives the offset in minutes, negative for `'-'`.
 * @return @c true when a well-formed offset was consumed.
 */
[[nodiscard]] inline bool takeUtcOffsetMinutes(DateTimeTextScanner& scanner,
                                               int& signedMinutes) noexcept {
    bool negative = false;
    if (scanner.take('-')) {
        negative = true;
    } else if (!scanner.take('+')) {
        return false;
    }

    int value = 0, digits = 0;
    if (!scanner.takeDigits(1, 4, value, &digits)) return false;

    int hours = 0, minutes = 0;
    if (digits <= 2) {
        hours = value;
        if (scanner.take(':')) {
            if (!scanner.takeDigits(1, 2, minutes)) return false;
        }
    } else {
        hours   = value / 100;
        minutes = value % 100;
    }
    if (minutes >= 60) return false;

    signedMinutes = hours * 60 + minutes;
    if (negative) signedMinutes = -signedMinutes;
    return true;
}

} // namespace System::detail
