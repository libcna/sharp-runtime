// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

// SHARED INTERNAL header. Cookie parsing lives in Net while HTTP header values live in
// Net.Http.Headers, which already depends privately on Net. Keeping the one parser in Net's
// `detail` include path lets both layers use it without a reverse dependency or an eighth copy.
// The `detail` path is not a supported public API even though module include propagation makes
// the header physically reachable to the dependent component.
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
// **Historical #2125 checkpoint.** That ticket deliberately changed only whole-input consumption:
// its `sscanf` conversion string stayed verbatim with `%n` appended. The table below records the
// grammar at that checkpoint, not the current parser's final surface. #2130 subsequently added
// RFC 9110's RFC 850 and asctime forms, and #2360 added the remaining measured .NET leniency;
// those current implementations and their exact bounds are documented at their bodies below.
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
// Neither obsolete form was accepted *at #2125* -- the conversion string required a comma
// immediately after a three-letter day name. #2130 resolved that then-open question from the
// reference and widened the one shared parser rather than recreating per-consumer copies.

#include "System/DateTimeOffset.hpp"
#include "System/TimeSpan.hpp"

#include <array>
#include <cstddef>
#include <cctype>
#include <string>
#include <string_view>

namespace System::Net::Http::Headers::detail {

    /** @brief ASCII-only case-insensitive equality used by invariant HTTP date names. */
    inline bool AsciiEqualsIgnoreCase(std::string_view left, std::string_view right) noexcept {
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) {
            const auto fold = [](unsigned char c) noexcept {
                return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
            };
            if (fold(static_cast<unsigned char>(left[i])) !=
                fold(static_cast<unsigned char>(right[i])))
                return false;
        }
        return true;
    }

    /** @brief The three-letter month names, in order, as every HTTP-date form spells them. */
    inline int MonthIndexFromName(std::string_view name) {
        static constexpr std::array<const char*, 12> months = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        for (int i = 0; i < 12; ++i) {
            if (AsciiEqualsIgnoreCase(name, months[static_cast<std::size_t>(i)])) return i;
        }
        return -1;
    }

    /**
     * @brief Expands an RFC 850 two-digit year the way .NET's invariant calendar does.
     *
     * The runtime's invariant Gregorian policy is `TwoDigitYearMax == 2049`, so `00`..`49` are
     * 2000..2049 and `50`..`99` are 1950..1999. Keeping a second, obsolete 2029 cutoff here made
     * HTTP-date disagree with `Calendar::ToFourDigitYear`; both ends are pinned by tests.
     */
    inline int ExpandTwoDigitYear(int twoDigitYear) {
        return twoDigitYear <= 49 ? 2000 + twoDigitYear : 1900 + twoDigitYear;
    }

    /** @brief Builds the result and verifies an optional weekday against the calendar date. */
    inline bool BuildHttpDate(int year, int monthIndex, int day, int hour, int minute, int second,
                              System::DateTimeOffset& result, int expectedWeekday = -1) {
        if (monthIndex < 0) return false;
        try {
            const System::DateTimeOffset candidate(
                year, monthIndex + 1, day, hour, minute, second, System::TimeSpan::Zero);
            if (expectedWeekday >= 0 &&
                static_cast<int>(candidate.getDayOfWeekProperty()) != expectedWeekday)
                return false;
            result = candidate;
            return true;
        } catch (...) {
            return false;
        }
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
    // The same explicit cursor parses the RFC 1123 / RFC 850 required forms and the measured
    // lenient variants. This avoids passing attacker-controlled unbounded integers through
    // `scanf`: every numeric token has its format-defined lexical width before conversion.
    // -----------------------------------------------------------------------------------------

    /**
     * @brief The seven three-letter weekday names, for the `ddd` the other two shapes spell.
     *
     * Ticket #2376. .NET's `ddd` is `MatchAbbreviatedDayName`, which matches only these seven;
     * `%3[A-Za-z]` matches any three letters, so `"Xyz, 06 Nov 1994 08:49:37 GMT"` parsed here
     * and does not in .NET.
     */
    inline int AbbreviatedWeekdayIndex(std::string_view name) {
        static constexpr std::array<const char*, 7> days = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        for (int i = 0; i < 7; ++i) {
            if (AsciiEqualsIgnoreCase(name, days[static_cast<std::size_t>(i)])) return i;
        }
        return -1;
    }

    inline bool IsAbbreviatedWeekdayName(std::string_view name) {
        return AbbreviatedWeekdayIndex(name) >= 0;
    }

    /** @brief The seven full weekday names, for the RFC 850 shape's `dddd`. */
    inline int FullWeekdayIndex(std::string_view name) {
        static constexpr std::array<const char*, 7> days = {
            "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        for (int i = 0; i < 7; ++i) {
            if (AsciiEqualsIgnoreCase(name, days[static_cast<std::size_t>(i)])) return i;
        }
        return -1;
    }

    inline bool IsFullWeekdayName(std::string_view name) {
        return FullWeekdayIndex(name) >= 0;
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
                   ((text_[index_] >= 'A' && text_[index_] <= 'Z') ||
                    (text_[index_] >= 'a' && text_[index_] <= 'z')) &&
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
     * @brief Parses RFC 1123 / RFC 850 and the sixteen additional measured .NET forms.
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
        int expectedWeekday = -1;
        if (hyphenated) {
            // RFC 850: the weekday is REQUIRED and must be a full name; the year is two digits.
            if (!haveWeekday || (expectedWeekday = FullWeekdayIndex(weekday)) < 0) return false;
            if (yearWidth != 2) return false;
        } else {
            // RFC 1123 / RFC 5322: the weekday is optional and must be one of the seven
            // three-letter names. #2376: checking only the LENGTH accepted an invented
            // abbreviation, and .NET's `ddd` is MatchAbbreviatedDayName -- a name table, not a
            // width. The lenient arm needs the same rule as the strict ones, or a value the
            // same shared arm does not accept a value carrying an invented weekday.
            if (haveWeekday &&
                (expectedWeekday = AbbreviatedWeekdayIndex(weekday)) < 0)
                return false;
            // THE TWO MISSING CELLS. A two-digit year with a numeric offset is not in .NET's
            // list -- neither "ddd, d MMM yy H:m:s zzz" nor "d MMM yy H:m:s zzz" appears -- so
            // it is rejected rather than completed by symmetry.
            if (yearWidth == 2 && zone == HttpDateZone::NumericOffset) return false;
        }

        if (yearWidth == 2) year = ExpandTwoDigitYear(year);

        try {
            const System::DateTimeOffset candidate(
                year, monthIndex + 1, day, hour, minute, second, offset);
            if (expectedWeekday >= 0 &&
                static_cast<int>(candidate.getDayOfWeekProperty()) != expectedWeekday)
                return false;
            result = candidate;
            return true;
        } catch (...) {
            return false;
        }
    }

    /** @brief Parses ANSI C's asctime HTTP-date shape with bounded lexical fields. */
    inline bool TryParseAsctimeHttpDate(const std::string& s,
                                        System::DateTimeOffset& result) {
        HttpDateCursor cursor(s);
        cursor.skipWhite();

        std::string weekday;
        if (!cursor.takeLetters(3, weekday)) return false;
        const int expectedWeekday = AbbreviatedWeekdayIndex(weekday);
        if (expectedWeekday < 0) return false;

        const std::size_t beforeMonthSpace = cursor.position();
        cursor.skipWhite();
        if (cursor.position() == beforeMonthSpace) return false;
        std::string monthName;
        if (!cursor.takeLetters(3, monthName)) return false;

        const std::size_t beforeDaySpace = cursor.position();
        cursor.skipWhite();
        if (cursor.position() == beforeDaySpace) return false;
        int day = 0;
        if (!cursor.takeDigits(1, 2, day)) return false;

        const std::size_t beforeTimeSpace = cursor.position();
        cursor.skipWhite();
        if (cursor.position() == beforeTimeSpace) return false;
        int hour = 0, minute = 0, second = 0;
        if (!cursor.takeDigits(1, 2, hour) || !cursor.take(':') ||
            !cursor.takeDigits(1, 2, minute) || !cursor.take(':') ||
            !cursor.takeDigits(1, 2, second))
            return false;

        const std::size_t beforeYearSpace = cursor.position();
        cursor.skipWhite();
        if (cursor.position() == beforeYearSpace) return false;
        int year = 0, yearWidth = 0;
        if (!cursor.takeDigits(4, 4, year, &yearWidth) || yearWidth != 4) return false;
        cursor.skipWhite();
        if (!cursor.atEnd()) return false;

        return BuildHttpDate(year, MonthIndexFromName(monthName), day, hour, minute, second,
                             result, expectedWeekday);
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
     * **Ticket #2130 (resolved verification) — the obsolete forms are accepted.**
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
     * #2360 subsequently adopted .NET's sixteen additional measured formats. The explicit
     * cursor keeps the exact cross and its two deliberately absent cells visible while also
     * bounding every integer lexeme before conversion.
     *
     * `asctime` carries no zone; like .NET's `DateTimeStyles.AssumeUniversal`, it is read as UTC.
     */
    inline bool TryParseHttpDate(const std::string& s, System::DateTimeOffset& result) {
        // An embedded NUL is not text in any HTTP-date token and must never truncate the value.
        if (s.find('\0') != std::string::npos) return false;
        if (TryParseAsctimeHttpDate(s, result)) return true;
        return TryParseLenientHttpDate(s, result);
    }

    /**
     * @brief Parses a Set-Cookie Expires date without widening the HTTP header grammar.
     *
     * .NET's CookieParser uses ParseCookieDate rather than HttpDateParser. In addition to the
     * ordinary HTTP-date forms it accepts `d-MMM-yyyy H:m:s GMT` and the two-digit-year variant
     * with no weekday. Keep that extra shape behind this cookie-only door: accepting it for Date,
     * Retry-After or If-Range would silently broaden those contracts.
     */
    inline bool TryParseCookieDate(const std::string& s, System::DateTimeOffset& result) {
        if (TryParseHttpDate(s, result)) return true;
        if (s.find('\0') != std::string::npos) return false;

        HttpDateCursor cursor(s);
        cursor.skipWhite();
        int day = 0;
        if (!cursor.takeDigits(1, 2, day) || !cursor.take('-')) return false;
        std::string monthName;
        if (!cursor.takeLetters(3, monthName) || !cursor.take('-')) return false;
        int year = 0, yearWidth = 0;
        if (!cursor.takeDigits(2, 4, year, &yearWidth) ||
            (yearWidth != 2 && yearWidth != 4))
            return false;

        const std::size_t beforeTimeSpace = cursor.position();
        cursor.skipWhite();
        if (cursor.position() == beforeTimeSpace) return false;
        int hour = 0, minute = 0, second = 0;
        if (!cursor.takeDigits(1, 2, hour) || !cursor.take(':') ||
            !cursor.takeDigits(1, 2, minute) || !cursor.take(':') ||
            !cursor.takeDigits(1, 2, second))
            return false;

        const std::size_t beforeZoneSpace = cursor.position();
        cursor.skipWhite();
        if (cursor.position() == beforeZoneSpace) return false;
        std::string zone;
        if (!cursor.takeLetters(3, zone) || zone != "GMT") return false;
        cursor.skipWhite();
        if (!cursor.atEnd()) return false;

        if (yearWidth == 2) year = ExpandTwoDigitYear(year);
        return BuildHttpDate(year, MonthIndexFromName(monthName), day, hour, minute, second,
                             result);
    }

} // namespace System::Net::Http::Headers::detail
