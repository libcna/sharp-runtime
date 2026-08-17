// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

// IMPLEMENTATION-ONLY header. It sits beside the sources under src/, not under include/, so it
// never becomes part of this component's public surface and is included with a plain relative
// path -- the same placement and spelling `modules/xml/src` already uses for
// `XPathAstInternal.hpp` and `XmlNodeChangeEvents.hpp`.
//
// Ticket #2125 (SR-AUD-321, cause NH-H, docs/SystemNetHttpHeadersNamespaceReviewPlan.md §4.3).
//
// The module carried **seven** byte-identical copies of an `sscanf`-based HTTP-date parser --
// §4.3 counted six and did not name `WarningHeaderValue`'s date field -- and none of them
// checked that the whole value had been consumed. So
//
//     RetryConditionHeaderValue::Parse("Sun, 06 Nov 1994 08:49:37 GMT trailing")
//
// succeeded and silently discarded ` trailing`, at every one of the seven doors. The control
// that proves this is a *consumption* defect rather than a permissive grammar is that
// `"garbage"` was, and still is, rejected.
//
// **What this deliberately does NOT change: the accepted grammar.** The `sscanf` conversion
// string is kept verbatim and `%n` is appended, so a value that parsed before parses to exactly
// the same instant now, and a value that failed before still fails. Rewriting this as a
// hand-written fixed-width scanner would additionally reject things `sscanf` accepts (a signed
// or over-wide day field, for instance), and with `/rv/tmp/runtime/` absent there is no evidence
// in this repository for what .NET does with those. #2125's job is full consumption; narrowing
// the grammar is not authorised and is not done here.
//
// **The obsolete formats, measured rather than assumed** (this is the pin #2125's acceptance
// criteria require and the measurement #2130 asked for on the port side):
//
//   | Format | Example | Before #2125 | After #2125 |
//   |---|---|---|---|
//   | IMF-fixdate (preferred) | `Sun, 06 Nov 1994 08:49:37 GMT` | parsed | parsed |
//   | RFC 850 (obsolete)      | `Sunday, 06-Nov-94 08:49:37 GMT` | **rejected** | **rejected** |
//   | ANSI C asctime (obsolete)| `Sun Nov  6 08:49:37 1994`      | **rejected** | **rejected** |
//
// Neither obsolete form was ever accepted -- the conversion string requires a comma immediately
// after a three-letter day name, which both obsolete forms fail -- so #2125 cannot have narrowed
// one away. RFC 9110 §5.6.7 requires a *recipient* to accept all three; whether .NET's parser
// does is the still-open question **#2130**, and it is a widening, not something this ticket
// may guess at.

#include "SharpRuntime/PortableScan.hpp"
#include "System/DateTimeOffset.hpp"
#include "System/TimeSpan.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace System::Net::Http::Headers::detail {

    /**
     * @brief Parses one HTTP-date in the preferred IMF-fixdate form, consuming the whole value.
     *
     * @param s The candidate field value.
     * @param result Receives the parsed instant (UTC) on success.
     * @return true only if @p s is a complete HTTP-date. Trailing text after the `GMT` token
     * makes the value invalid; trailing *whitespace* does not, because whitespace was accepted
     * before #2125 and is not what the finding is about.
     */
    /** @brief The three-letter month names, in order, as every HTTP-date form spells them. */
    inline int MonthIndexFromName(const char* name) {
        static constexpr std::array<const char*, 12> months = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        for (int i = 0; i < 12; ++i) {
            if (std::strcmp(name, months[static_cast<size_t>(i)]) == 0) return i;
        }
        return -1;
    }

    /**
     * @brief Expands an RFC 850 two-digit year the way .NET's invariant calendar does.
     *
     * Ticket #2130. `DateTimeFormatInfo.InvariantInfo`'s Gregorian calendar has
     * `TwoDigitYearMax == 2029`, so `00`..`29` are 2000..2029 and `30`..`99` are 1930..1999.
     * Getting this wrong is the easy mistake in RFC 850 support -- a naive `1900 + yy` turns
     * `Sunday, 06-Nov-06 ...` into 1906 -- so it is pinned by a test at both ends of the window.
     */
    inline int ExpandTwoDigitYear(int twoDigitYear) {
        return twoDigitYear <= 29 ? 2000 + twoDigitYear : 1900 + twoDigitYear;
    }

    /** @brief Builds the result, returning false for a date the calendar rejects. */
    inline bool BuildHttpDate(int year, int monthIndex, int day, int hour, int minute, int second,
                              System::DateTimeOffset& result) {
        if (monthIndex < 0) return false;
        try {
            result = System::DateTimeOffset(year, monthIndex + 1, day, hour, minute, second,
                                            System::TimeSpan::Zero);
            return true;
        } catch (...) {
            return false;
        }
    }

    /** @brief True when everything from @p from to the end of @p s is whitespace. */
    inline bool OnlyTrailingWhitespace(const std::string& s, int from) {
        if (from < 0) return false;
        for (size_t i = static_cast<size_t>(from); i < s.size(); ++i) {
            if (std::isspace(static_cast<unsigned char>(s[i])) == 0) return false;
        }
        return true;
    }

    /**
     * @brief Parses one HTTP-date in any of RFC 9110 §5.6.7's three forms, consuming the whole
     *        value.
     *
     * @param s The candidate field value.
     * @param result Receives the parsed instant (UTC) on success.
     * @return true only if @p s is a complete HTTP-date. Trailing text makes the value invalid;
     *         trailing *whitespace* does not, which is #2125's rule and is unchanged.
     *
     * ---
     *
     * **Ticket #2130 (deferred verification) — the obsolete forms are now accepted.**
     *
     * #2125 recorded, correctly, that neither obsolete form had *ever* been accepted here, so it
     * could not have narrowed one away; and that whether .NET accepts them "is the still-open
     * question #2130, and it is a widening, not something this ticket may guess at". The
     * reference settles it: `HttpDateParser.TryParse` tries strict `"r"` and then **twenty-one**
     * format strings, of which four are RFC 850 and one is ANSI C's `asctime`
     * (`Common/src/System/Net/HttpDateParser.cs:9-32`).
     *
     * RFC 9110 §5.6.7 requires a **recipient** to accept all three, so all three are accepted:
     *
     *   | Form | Example |
     *   |---|---|
     *   | IMF-fixdate (preferred) | `Sun, 06 Nov 1994 08:49:37 GMT` |
     *   | RFC 850 (obsolete) | `Sunday, 06-Nov-94 08:49:37 GMT` |
     *   | ANSI C `asctime` (obsolete) | `Sun Nov  6 08:49:37 1994` |
     *
     * **What is deliberately NOT adopted, and it is most of .NET's list.** The remaining sixteen
     * formats are *leniency*, not required forms: a `UTC` zone token instead of `GMT`, no zone
     * token at all, a missing day-of-week, a two-digit year on an IMF-fixdate, and RFC 5322
     * numeric offsets. Adopting them would accept text RFC 9110 does not define as an HTTP-date,
     * which is a much larger widening than the one this ticket asked for, and each has its own
     * ambiguities (a bare `08:49:37` with no zone is only UTC because .NET *assumes* it is). That
     * gap is recorded as ticket **#2360** rather than smuggled in here.
     *
     * `asctime` carries no zone; like .NET's `DateTimeStyles.AssumeUniversal`, it is read as UTC.
     */
    inline bool TryParseHttpDate(const std::string& s, System::DateTimeOffset& result) {
        // An embedded NUL would let sscanf stop early and report a complete match over a prefix,
        // so it is rejected before c_str() hides the rest of the value (#2125).
        if (s.find('\0') != std::string::npos) return false;

        char dayName[16] = {};
        char monthName[4] = {};
        int day = 0, year = 0, hour = 0, minute = 0, second = 0;
        char zone[4] = {};
        int consumed = -1;

        // 1. IMF-fixdate -- the preferred form, and the only one this parser used to accept.
        //    The conversion string is #2125's verbatim apart from the two %n markers that
        //    bracket the year, so a value that parsed before parses to exactly the same instant
        //    now -- with ONE correction, below.
        int yearBegin = -1, yearEnd = -1;
        if (SHARP_RUNTIME_SSCANF(
                s.c_str(), "%3[A-Za-z], %d %3[A-Za-z] %n%d%n %d:%d:%d %3s%n",
                SHARP_RUNTIME_SCANF_BUFFER(dayName), &day, SHARP_RUNTIME_SCANF_BUFFER(monthName),
                &yearBegin, &year, &yearEnd, &hour, &minute, &second,
                SHARP_RUNTIME_SCANF_BUFFER(zone), &consumed) == 8 &&
            std::strcmp(zone, "GMT") == 0 && OnlyTrailingWhitespace(s, consumed)) {
            // #2130, and this is a CORRECTION rather than a widening. `%d` read a two-digit year
            // literally, so "Sun, 06 Nov 94 08:49:37 GMT" was ACCEPTED and reported the year
            // **94 AD** -- a silently wrong instant, off by nineteen centuries, not a rejected
            // format. .NET accepts the same text and reads 1994 ("ddd, d MMM yy H:m:s 'GMT'",
            // HttpDateParser.cs:17), so the port's answer was wrong rather than merely strict.
            //
            // Only an exactly-two-digit token is expanded. Every other width keeps the value it
            // had, so nothing else moves.
            if (yearBegin >= 0 && yearEnd == yearBegin + 2 && year >= 0 && year <= 99) {
                year = ExpandTwoDigitYear(year);
            }
            return BuildHttpDate(year, MonthIndexFromName(monthName), day, hour, minute, second,
                                 result);
        }

        // 2. RFC 850 -- a FULL weekday name, hyphen-separated date, two-digit year.
        //    HttpDateParser.cs:22 -- "dddd, d'-'MMM'-'yy H:m:s 'GMT'".
        std::memset(dayName, 0, sizeof(dayName));
        std::memset(monthName, 0, sizeof(monthName));
        std::memset(zone, 0, sizeof(zone));
        consumed = -1;
        int twoDigitYear = 0;
        if (SHARP_RUNTIME_SSCANF(
                s.c_str(), "%15[A-Za-z], %d-%3[A-Za-z]-%d %d:%d:%d %3s%n",
                SHARP_RUNTIME_SCANF_BUFFER(dayName), &day, SHARP_RUNTIME_SCANF_BUFFER(monthName),
                &twoDigitYear, &hour, &minute, &second, SHARP_RUNTIME_SCANF_BUFFER(zone),
                &consumed) == 8 &&
            std::strcmp(zone, "GMT") == 0 && OnlyTrailingWhitespace(s, consumed) &&
            twoDigitYear >= 0 && twoDigitYear <= 99) {
            return BuildHttpDate(ExpandTwoDigitYear(twoDigitYear), MonthIndexFromName(monthName),
                                 day, hour, minute, second, result);
        }

        // 3. ANSI C asctime -- no zone, no comma, and a SPACE-PADDED day.
        //    HttpDateParser.cs:26 -- "ddd MMM d H:m:s yyyy". sscanf's %d already skips the
        //    padding, so "Nov  6" and "Nov 16" both work without a second conversion string.
        std::memset(dayName, 0, sizeof(dayName));
        std::memset(monthName, 0, sizeof(monthName));
        consumed = -1;
        if (SHARP_RUNTIME_SSCANF(
                s.c_str(), "%3[A-Za-z] %3[A-Za-z] %d %d:%d:%d %d%n",
                SHARP_RUNTIME_SCANF_BUFFER(dayName), SHARP_RUNTIME_SCANF_BUFFER(monthName), &day,
                &hour, &minute, &second, &year, &consumed) == 7 &&
            OnlyTrailingWhitespace(s, consumed)) {
            return BuildHttpDate(year, MonthIndexFromName(monthName), day, hour, minute, second,
                                 result);
        }

        return false;
    }

} // namespace System::Net::Http::Headers::detail
