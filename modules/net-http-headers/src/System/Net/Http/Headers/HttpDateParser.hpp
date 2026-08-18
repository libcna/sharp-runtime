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


    // -----------------------------------------------------------------------------------------
    // Ticket #2360 (2026-08-18) -- the sixteen LENIENT formats.
    //
    // #2130 adopted RFC 9110 5.6.7's three required forms and recorded the rest as "leniency,
    // not required forms ... recorded as ticket #2360 rather than smuggled in here". This is that
    // ticket. .NET's HttpDateParser tries strict "r" and then TWENTY-ONE format strings
    // (Common/src/System/Net/HttpDateParser.cs:9-32) with
    // DateTimeStyles.AllowInnerWhite | AssumeUniversal, and a recipient that rejects what .NET
    // accepts will drop responses .NET's own clients read.
    //
    // Twenty-one format strings are not twenty-one grammars. They are three shapes crossed with
    // three axes, and transcribing the CROSS rather than the list is what makes the gaps visible:
    //
    //   shape      day-of-week        date            year     zone
    //   -------    ---------------    ------------    -----    ------------------------
    //   RFC 1123   "Ddd," or absent   d MMM yyyy      4 or 2    GMT | UTC | zzz | absent
    //   RFC 850    "Dddddd," only     d-MMM-yy        2 only    GMT | UTC | zzz | absent
    //   asctime    "Ddd" no comma     MMM d ... yyyy  4 only    absent only
    //
    // TWO CELLS OF THAT CROSS ARE MISSING FROM .NET'S LIST and are therefore rejected here: a
    // two-digit year combined with a numeric offset, with or without a day-of-week. Writing the
    // table as a cross would have silently ADDED those two; they are excluded explicitly and
    // pinned, because "the obvious completion of the pattern" is exactly the kind of widening
    // that has no reference behind it.
    //
    // The zone token comes from .NET's `zzz` specifier, whose parser is ParseTimeZoneOffset
    // (DateTimeParse.cs:3285-3345): a sign, one or two hour digits, then an OPTIONAL ':' before
    // two minute digits -- so "-05:00" and "-0500" are both accepted, and the minute field is
    // rejected at 60. A missing zone means UTC, which is DateTimeStyles.AssumeUniversal rather
    // than an assumption of this port's.
    //
    // This runs AFTER the three strict arms above, and that ordering is the safety property: any
    // value they accept never reaches here, so no already-parsing input can change its answer.
    // It is a pure widening.
    // -----------------------------------------------------------------------------------------

    /**
     * @brief The seven three-letter weekday names, for the `ddd` the other two shapes spell.
     *
     * Ticket #2376. .NET's `ddd` is `MatchAbbreviatedDayName`, which matches only these seven;
     * `%3[A-Za-z]` matches any three letters, so `"Xyz, 06 Nov 1994 08:49:37 GMT"` parsed here
     * and does not in .NET.
     */
    inline bool IsAbbreviatedWeekdayName(const std::string& name) {
        static constexpr std::array<const char*, 7> days = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        for (const char* d : days) {
            if (name == d) return true;
        }
        return false;
    }

    /** @brief The seven full weekday names, for the RFC 850 shape's `dddd`. */
    inline bool IsFullWeekdayName(const std::string& name) {
        static constexpr std::array<const char*, 7> days = {
            "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        for (const char* d : days) {
            if (name == d) return true;
        }
        return false;
    }

    /** @brief A forward-only cursor, deliberately not `sscanf`: `%d` accepts a sign and `+3` is not a day. */
    class HttpDateCursor {
    public:
        explicit HttpDateCursor(const std::string& text) noexcept : text_(text) {}

        [[nodiscard]] bool atEnd() const noexcept { return index_ >= text_.size(); }
        [[nodiscard]] std::size_t position() const noexcept { return index_; }
        void seek(std::size_t position) noexcept { index_ = position; }

        /** @brief Consumes any run of whitespace; `DateTimeStyles.AllowInnerWhite`. */
        void skipWhite() noexcept {
            while (index_ < text_.size() &&
                   std::isspace(static_cast<unsigned char>(text_[index_])) != 0)
                ++index_;
        }

        bool take(char c) noexcept {
            if (index_ < text_.size() && text_[index_] == c) { ++index_; return true; }
            return false;
        }

        /** @brief Consumes between @p minDigits and @p maxDigits ASCII digits, reporting the width. */
        bool takeDigits(int minDigits, int maxDigits, int& value, int* width = nullptr) noexcept {
            const std::size_t start = index_;
            int accumulated = 0, count = 0;
            while (index_ < text_.size() && count < maxDigits &&
                   text_[index_] >= '0' && text_[index_] <= '9') {
                accumulated = accumulated * 10 + (text_[index_] - '0');
                ++index_; ++count;
            }
            if (count < minDigits) { index_ = start; return false; }
            value = accumulated;
            if (width != nullptr) *width = count;
            return true;
        }

        /** @brief Consumes a run of ASCII letters, at most @p maxLetters of them. */
        bool takeLetters(std::size_t maxLetters, std::string& out) {
            const std::size_t start = index_;
            while (index_ < text_.size() &&
                   std::isalpha(static_cast<unsigned char>(text_[index_])) != 0 &&
                   index_ - start < maxLetters)
                ++index_;
            if (index_ == start) return false;
            out.assign(text_, start, index_ - start);
            return true;
        }

    private:
        const std::string& text_;
        std::size_t        index_ = 0;
    };

    /** @brief The zone axis, as .NET's four possibilities. */
    enum class HttpDateZone { Gmt, Utc, NumericOffset, Absent };

    /**
     * @brief Consumes an optional zone token, transcribing .NET's `zzz` where one is numeric.
     *
     * @param cursor Positioned after the seconds field.
     * @param kind   Receives which of the four possibilities was found.
     * @param offset Receives the offset; `TimeSpan::Zero` for `GMT`, `UTC` and absent, the latter
     *               being `DateTimeStyles.AssumeUniversal` rather than a guess.
     * @return @c false only for text that starts a zone token and then fails to be one.
     */
    inline bool TakeHttpDateZone(HttpDateCursor& cursor, HttpDateZone& kind,
                                 System::TimeSpan& offset) {
        kind   = HttpDateZone::Absent;
        offset = System::TimeSpan::Zero;

        const std::size_t beforeWhite = cursor.position();
        cursor.skipWhite();
        if (cursor.atEnd()) { cursor.seek(beforeWhite); return true; }

        const std::size_t start = cursor.position();
        std::string       token;
        if (cursor.takeLetters(3, token)) {
            if (token == "GMT") { kind = HttpDateZone::Gmt; return true; }
            if (token == "UTC") { kind = HttpDateZone::Utc; return true; }
            cursor.seek(start);
            return false;
        }

        bool negative = false;
        if (cursor.take('-')) {
            negative = true;
        } else if (!cursor.take('+')) {
            cursor.seek(beforeWhite);
            return true;   // not a zone token at all; the trailing-text check will judge it
        }

        int hours = 0, minutes = 0;
        if (!cursor.takeDigits(1, 2, hours)) return false;
        (void)cursor.take(':');                       // ':' is optional -- DateTimeParse.cs:3315
        if (!cursor.takeDigits(2, 2, minutes)) return false;
        if (minutes >= 60) return false;              // DateTimeParse.cs:3334

        kind   = HttpDateZone::NumericOffset;
        offset = System::TimeSpan::FromMinutes(negative ? -(hours * 60 + minutes)
                                                        : (hours * 60 + minutes));
        return true;
    }

    /**
     * @brief Parses the sixteen lenient forms .NET accepts beyond RFC 9110's three.
     *
     * Runs only after the three strict arms have declined, so it can never change an answer an
     * already-accepted value had.
     */
    inline bool TryParseLenientHttpDate(const std::string& s, System::DateTimeOffset& result) {
        HttpDateCursor cursor(s);
        cursor.skipWhite();

        // The day-of-week is optional, and WHICH forms may carry which spelling depends on the
        // date separator, which has not been seen yet. So it is captured now and judged below.
        std::string       weekday;
        const std::size_t beforeWeekday = cursor.position();
        bool              haveWeekday   = false;
        if (cursor.takeLetters(9, weekday)) {
            cursor.skipWhite();
            if (cursor.take(',')) {
                haveWeekday = true;
            } else {
                cursor.seek(beforeWeekday);   // an asctime-shaped value, or no weekday at all
                weekday.clear();
            }
        }
        cursor.skipWhite();

        int day = 0;
        if (!cursor.takeDigits(1, 2, day)) return false;

        // The separator picks the shape: '-' is RFC 850, whitespace is RFC 1123 / RFC 5322.
        bool hyphenated = false;
        if (cursor.take('-')) {
            hyphenated = true;
        } else {
            const std::size_t beforeSpace = cursor.position();
            cursor.skipWhite();
            if (cursor.position() == beforeSpace) return false;
        }

        std::string monthName;
        if (!cursor.takeLetters(3, monthName)) return false;
        const int monthIndex = MonthIndexFromName(monthName.c_str());
        if (monthIndex < 0) return false;

        if (hyphenated) {
            if (!cursor.take('-')) return false;
        } else {
            const std::size_t beforeSpace = cursor.position();
            cursor.skipWhite();
            if (cursor.position() == beforeSpace) return false;
        }

        int year = 0, yearWidth = 0;
        if (!cursor.takeDigits(1, 4, year, &yearWidth)) return false;
        if (yearWidth != 2 && yearWidth != 4) return false;   // .NET's yy and yyyy are exact

        const std::size_t beforeTimeSpace = cursor.position();
        cursor.skipWhite();
        if (cursor.position() == beforeTimeSpace) return false;

        int hour = 0, minute = 0, second = 0;
        if (!cursor.takeDigits(1, 2, hour) || !cursor.take(':') ||
            !cursor.takeDigits(1, 2, minute) || !cursor.take(':') ||
            !cursor.takeDigits(1, 2, second))
            return false;

        HttpDateZone     zone{};
        System::TimeSpan offset = System::TimeSpan::Zero;
        if (!TakeHttpDateZone(cursor, zone, offset)) return false;

        cursor.skipWhite();
        if (!cursor.atEnd()) return false;   // trailing text, which #2125 made a failure

        // Now judge the combination against .NET's twenty-one, rather than against the cross.
        if (hyphenated) {
            // RFC 850: the weekday is REQUIRED and must be a full name; the year is two digits.
            if (!haveWeekday || !IsFullWeekdayName(weekday)) return false;
            if (yearWidth != 2) return false;
        } else {
            // RFC 1123 / RFC 5322: the weekday is optional and must be one of the seven
            // three-letter names. #2376: checking only the LENGTH accepted an invented
            // abbreviation, and .NET's `ddd` is MatchAbbreviatedDayName -- a name table, not a
            // width. The lenient arm needs the same rule as the strict ones, or a value the
            // strict arm has just refused for a bad weekday is accepted here instead.
            if (haveWeekday && !IsAbbreviatedWeekdayName(weekday)) return false;
            // THE TWO MISSING CELLS. A two-digit year with a numeric offset is not in .NET's
            // list -- neither "ddd, d MMM yy H:m:s zzz" nor "d MMM yy H:m:s zzz" appears -- so
            // it is rejected rather than completed by symmetry.
            if (yearWidth == 2 && zone == HttpDateZone::NumericOffset) return false;
        }

        if (yearWidth == 2) year = ExpandTwoDigitYear(year);

        try {
            result = System::DateTimeOffset(year, monthIndex + 1, day, hour, minute, second, offset);
            return true;
        } catch (...) {
            return false;
        }
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
            std::strcmp(zone, "GMT") == 0 && OnlyTrailingWhitespace(s, consumed) &&
            // Ticket #2376. sscanf's %d and %3[A-Za-z] do not bound a field, so this arm was
            // WIDER than the format string it transcribes. .NET's `ddd` is MatchAbbreviatedDayName
            // (seven names, not any three letters) and its `yyyy`/`yy` are ParseDigits with an
            // EXACT width -- so "Sun, 06 Nov 199 08:49:37 GMT" parsed here as the year 199 AD and
            // does not parse in .NET at all.
            IsAbbreviatedWeekdayName(dayName) && yearBegin >= 0 &&
            (yearEnd == yearBegin + 2 || yearEnd == yearBegin + 4)) {
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
            twoDigitYear >= 0 && twoDigitYear <= 99 &&
            // #2376. .NET spells this shape's weekday `dddd` (HttpDateParser.cs:22-25), which
            // matches a FULL name only, so "Sun, 06-Nov-94 08:49:37 GMT" parsed here and does not
            // in .NET. %15[A-Za-z] accepts any run of up to fifteen letters.
            IsFullWeekdayName(dayName)) {
            return BuildHttpDate(ExpandTwoDigitYear(twoDigitYear), MonthIndexFromName(monthName),
                                 day, hour, minute, second, result);
        }

        // 3. ANSI C asctime -- no zone, no comma, and a SPACE-PADDED day.
        //    HttpDateParser.cs:26 -- "ddd MMM d H:m:s yyyy". sscanf's %d already skips the
        //    padding, so "Nov  6" and "Nov 16" both work without a second conversion string.
        std::memset(dayName, 0, sizeof(dayName));
        std::memset(monthName, 0, sizeof(monthName));
        consumed = -1;
        int asctimeYearBegin = -1, asctimeYearEnd = -1;
        if (SHARP_RUNTIME_SSCANF(
                s.c_str(), "%3[A-Za-z] %3[A-Za-z] %d %d:%d:%d %n%d%n",
                SHARP_RUNTIME_SCANF_BUFFER(dayName), SHARP_RUNTIME_SCANF_BUFFER(monthName), &day,
                &hour, &minute, &second, &asctimeYearBegin, &year, &asctimeYearEnd) == 7 &&
            OnlyTrailingWhitespace(s, asctimeYearEnd) &&
            // #2376, the third site of the same defect. asctime's year is `yyyy` -- exactly four
            // digits -- and its weekday is `ddd` (HttpDateParser.cs:26). The ticket named two
            // sites and there are three; leaving the third would repair two thirds of one rule.
            IsAbbreviatedWeekdayName(dayName) && asctimeYearBegin >= 0 &&
            asctimeYearEnd == asctimeYearBegin + 4) {
            return BuildHttpDate(year, MonthIndexFromName(monthName), day, hour, minute, second,
                                 result);
        }

        // 4. #2360 -- the sixteen lenient forms, tried last so the three strict arms above keep
        //    every answer they already gave.
        return TryParseLenientHttpDate(s, result);
    }

} // namespace System::Net::Http::Headers::detail
