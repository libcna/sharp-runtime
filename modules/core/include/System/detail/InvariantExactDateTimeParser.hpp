// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <array>
#include <cstddef>
#include <string>

/**
 * @file
 * @brief The invariant exact-format scanner behind `DateOnly`/`TimeOnly` `ParseExact`
 *        (ticket #1939, #1929 row 4A).
 *
 * **This is deliberately smaller than .NET and does not pretend to be culture-aware.** The
 * approval in `docs/DateTimeExactParsingAndKindDesign.md` §"4A exact selected contract" names the
 * complete token subset, and everything outside it is rejected rather than approximated: eras,
 * calendars, zones, the `/` and `:` culture separator placeholders, provider overloads, styles,
 * multi-format and span APIs.
 *
 * Field widths are .NET's, and the rule is not "the number of specifiers": `ParseDigits(str, 1)`
 * parses **one or two** digits while `ParseDigits(str, n)` for `n > 1` parses **exactly** `n`
 * (`DateTimeParse.cs`). So `d` accepts `5` and `05`, and `dd` accepts only `05` — which is why a
 * scanner written as "count the specifiers, read that many digits" gets `d` wrong.
 */
namespace System::detail {

    /** @brief The fields an exact format may bind, and whether it bound them. */
    struct ExactDateTimeFields {
        int  year = -1, month = -1, day = -1;
        int  hour = -1, minute = -1, second = -1;
        long fractionTicks = 0;
        int  weekday = -1;     ///< 0 = Sunday, from `ddd`/`dddd`; -1 when absent.
        int  amPm    = -1;     ///< 0 = AM, 1 = PM, from `t`/`tt`; -1 when absent.
        bool twelveHour = false;
    };

    /** @brief Invariant English month names, abbreviated and full, in calendar order. */
    inline constexpr std::array<const char*, 12> kAbbreviatedMonths = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    inline constexpr std::array<const char*, 12> kFullMonths = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"};
    inline constexpr std::array<const char*, 7> kAbbreviatedDays = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    inline constexpr std::array<const char*, 7> kFullDays = {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

    /**
     * @brief Expands the two invariant standard formats each type supports.
     *
     * `O`/`o` and `R`/`r` only. A single-character format that is not one of those is **not** a
     * standard format and is **not** silently treated as a custom one either: .NET requires `%d`
     * for a one-character custom format, because a bare `d` would be the standard short-date
     * pattern. That distinction is preserved.
     *
     * @return The expanded custom pattern, or an empty string when @p format is not standard.
     */
    [[nodiscard]] inline std::string ExpandStandardFormat(const std::string& format,
                                                          bool forDate) {
        if (format.size() != 1) return {};
        switch (format[0]) {
            case 'O': case 'o': return forDate ? "yyyy-MM-dd" : "HH:mm:ss.fffffff";
            case 'R': case 'r': return forDate ? "ddd, dd MMM yyyy" : "HH:mm:ss";
            default:            return {};
        }
    }

    /** @brief A forward cursor over the input, with .NET's digit-width rule. */
    class ExactInputCursor {
    public:
        explicit ExactInputCursor(const std::string& text) noexcept : text_(text) {}
        [[nodiscard]] bool        atEnd() const noexcept { return index_ >= text_.size(); }
        [[nodiscard]] std::size_t position() const noexcept { return index_; }

        bool take(char c) noexcept {
            if (index_ < text_.size() && text_[index_] == c) { ++index_; return true; }
            return false;
        }

        /**
         * @brief Reads a numeric field of the width .NET's `ParseDigits` would read.
         *
         * @param specifierCount How many specifiers the format used: 1 means "one or two
         *                       digits", anything larger means "exactly that many".
         */
        bool takeNumber(int specifierCount, int& value) noexcept {
            const int minDigits = specifierCount;
            const int maxDigits = specifierCount == 1 ? 2 : specifierCount;
            int accumulated = 0, count = 0;
            const std::size_t start = index_;
            while (index_ < text_.size() && count < maxDigits &&
                   text_[index_] >= '0' && text_[index_] <= '9') {
                accumulated = accumulated * 10 + (text_[index_] - '0');
                ++index_; ++count;
            }
            if (count < minDigits) { index_ = start; return false; }
            value = accumulated;
            return true;
        }

        /** @brief Consumes exactly @p digits digits, for a fixed-width `f` fraction. */
        bool takeFixedDigits(int digits, long& value) noexcept {
            const std::size_t start = index_;
            long accumulated = 0;
            for (int i = 0; i < digits; ++i) {
                if (index_ >= text_.size() || text_[index_] < '0' || text_[index_] > '9') {
                    index_ = start;
                    return false;
                }
                accumulated = accumulated * 10 + (text_[index_] - '0');
                ++index_;
            }
            value = accumulated;
            return true;
        }

        /** @brief Consumes up to @p maxDigits digits, for a trailing-optional `F` fraction. */
        int takeUpToDigits(int maxDigits, long& value) noexcept {
            long accumulated = 0;
            int  count = 0;
            while (count < maxDigits && index_ < text_.size() &&
                   text_[index_] >= '0' && text_[index_] <= '9') {
                accumulated = accumulated * 10 + (text_[index_] - '0');
                ++index_; ++count;
            }
            value = accumulated;
            return count;
        }

        /** @brief Consumes @p word if the input continues with it, case-sensitively. */
        bool takeWord(const char* word) noexcept {
            const std::size_t len = std::char_traits<char>::length(word);
            if (text_.compare(index_, len, word) != 0) return false;
            index_ += len;
            return true;
        }

    private:
        const std::string& text_;
        std::size_t        index_ = 0;
    };

    /**
     * @brief Matches @p format against @p input, binding the fields it names.
     *
     * @param forDate  Selects the token set: the date tokens (`y M d`, names) or the time tokens
     *                 (`H h m s f F t`). A token from the other set is a MALFORMED FORMAT, not an
     *                 input mismatch, and both report the same `FormatException` family here --
     *                 the approval says so explicitly, and .NET agrees.
     * @return @c false for a malformed or unsupported format, or for input that does not match.
     *
     * Whitespace in the format matches the same whitespace in the input literally, and nothing
     * else: the approval requires "complete input consumption" and rejects "leading, trailing, or
     * extra inner whitespace". There is no `AllowLeadingWhite` here, because styles are 4C.
     */
    [[nodiscard]] inline bool MatchExactFormat(const std::string& input, const std::string& format,
                                               bool forDate, ExactDateTimeFields& fields) {
        if (format.empty()) return false;
        ExactInputCursor cursor(input);

        const auto runLength = [&format](std::size_t at) {
            std::size_t n = 1;
            while (at + n < format.size() && format[at + n] == format[at]) ++n;
            return n;
        };
        const auto matchName = [&cursor](const auto& names, int& out) {
            // Longest first, so a name that is a prefix of another cannot win.
            //
            // HONEST NOTE ON THE EVIDENCE: for the four INVARIANT tables this is defensive rather
            // than load-bearing, and a mutation taking the first match instead is NOT caught --
            // measured. No invariant month or day name is a prefix of another, and the abbreviated
            // and full tables are never consulted in the same call. It is kept because the rule is
            // what makes the loop correct for any table, not because a test distinguishes it.
            int  best = -1;
            std::size_t bestLen = 0;
            for (std::size_t i = 0; i < names.size(); ++i) {
                const std::size_t len = std::char_traits<char>::length(names[i]);
                if (len <= bestLen) continue;
                ExactInputCursor probe = cursor;
                if (probe.takeWord(names[i])) { best = static_cast<int>(i); bestLen = len; }
            }
            if (best < 0) return false;
            (void)cursor.takeWord(names[static_cast<std::size_t>(best)]);
            out = best;
            return true;
        };

        for (std::size_t i = 0; i < format.size();) {
            const char c = format[i];

            // The literal mechanisms, shared by both token sets.
            if (c == '%') {
                if (i + 1 >= format.size() || format[i + 1] == '%') return false;
                ++i;                       // the next specifier is taken as a single-char custom one
                continue;
            }
            if (c == '\\') {
                if (i + 1 >= format.size()) return false;
                if (!cursor.take(format[i + 1])) return false;
                i += 2;
                continue;
            }
            if (c == '\'' || c == '"') {
                const std::size_t close = format.find(c, i + 1);
                if (close == std::string::npos) return false;   // unterminated literal
                for (std::size_t k = i + 1; k < close; ++k) {
                    if (!cursor.take(format[k])) return false;
                }
                i = close + 1;
                continue;
            }

            const std::size_t run = runLength(i);
            const int         n   = static_cast<int>(run);

            if (forDate) {
                if (c == 'y') {
                    if (n > 4 || fields.year >= 0) return false;
                    int value = 0;
                    if (!cursor.takeNumber(n, value)) return false;
                    // The invariant calendar's TwoDigitYearMax is 2029, which is a FIXED value of
                    // the invariant culture rather than culture state -- so `yy` may use it here
                    // even though #1929 declined a two-digit year in the GENERAL parser, where the
                    // width is the discriminator and no format says what was meant.
                    fields.year = (n <= 2 && value <= 99) ? (value <= 29 ? 2000 + value : 1900 + value)
                                                          : value;
                    i += run;
                    continue;
                }
                if (c == 'M') {
                    if (n > 4 || fields.month >= 0) return false;
                    int value = 0;
                    if (n <= 2) {
                        if (!cursor.takeNumber(n, value)) return false;
                        fields.month = value;
                    } else {
                        int index = 0;
                        if (!matchName(n == 3 ? kAbbreviatedMonths : kFullMonths, index)) return false;
                        fields.month = index + 1;
                    }
                    i += run;
                    continue;
                }
                if (c == 'd') {
                    if (n <= 2) {
                        if (fields.day >= 0) return false;
                        int value = 0;
                        if (!cursor.takeNumber(n, value)) return false;
                        fields.day = value;
                    } else {
                        if (n > 4 || fields.weekday >= 0) return false;
                        int index = 0;
                        if (!matchName(n == 3 ? kAbbreviatedDays : kFullDays, index)) return false;
                        fields.weekday = index;
                    }
                    i += run;
                    continue;
                }
                // Any time or zone token in a date format is unsupported, not a literal.
                if (c == 'H' || c == 'h' || c == 'm' || c == 's' || c == 'f' || c == 'F' ||
                    c == 't' || c == 'z' || c == 'K' || c == 'g')
                    return false;
            } else {
                if (c == 'H' || c == 'h') {
                    if (n > 2 || fields.hour >= 0) return false;
                    int value = 0;
                    if (!cursor.takeNumber(n, value)) return false;
                    fields.hour       = value;
                    fields.twelveHour = (c == 'h');
                    i += run;
                    continue;
                }
                if (c == 'm') {
                    if (n > 2 || fields.minute >= 0) return false;
                    int value = 0;
                    if (!cursor.takeNumber(n, value)) return false;
                    fields.minute = value;
                    i += run;
                    continue;
                }
                if (c == 's') {
                    if (n > 2 || fields.second >= 0) return false;
                    int value = 0;
                    if (!cursor.takeNumber(n, value)) return false;
                    fields.second = value;
                    i += run;
                    continue;
                }
                if (c == 'f' || c == 'F') {
                    if (n > 7 || fields.fractionTicks != 0) return false;
                    long value = 0;
                    int  read  = n;
                    if (c == 'f') {
                        if (!cursor.takeFixedDigits(n, value)) return false;
                    } else {
                        read = cursor.takeUpToDigits(n, value);
                    }
                    for (int k = read; k < 7; ++k) value *= 10;
                    fields.fractionTicks = value;
                    i += run;
                    continue;
                }
                if (c == 't') {
                    if (n > 2 || fields.amPm >= 0) return false;
                    if (n == 1) {
                        if (cursor.take('A')) fields.amPm = 0;
                        else if (cursor.take('P')) fields.amPm = 1;
                        else return false;
                    } else {
                        if (cursor.takeWord("AM")) fields.amPm = 0;
                        else if (cursor.takeWord("PM")) fields.amPm = 1;
                        else return false;
                    }
                    i += run;
                    continue;
                }
                // Any date or zone token in a time format is unsupported.
                if (c == 'y' || c == 'M' || c == 'd' || c == 'z' || c == 'K' || c == 'g')
                    return false;
            }

            // Everything else is a literal, matched exactly once per format character.
            //
            // THAT INCLUDES ':' AND '/', which .NET treats as the culture's time and date
            // SEPARATOR PLACEHOLDERS. The approval says to reject "provider separator
            // placeholders", and rejecting the CHARACTERS would go further than it means: its own
            // worked example is `TryParseExact("10:20:30.12345678", "HH:mm:ss.ffffffff")`, which
            // must fail on the eight `f`s and not on the colons. For the invariant culture the
            // placeholders resolve to exactly ':' and '/', so matching them literally is the same
            // answer; what is rejected is the ability of a PROVIDER to change them, which is 4B
            // and is out of scope by name.
            for (std::size_t k = 0; k < run; ++k) {
                if (!cursor.take(c)) return false;
            }
            i += run;
        }

        return cursor.atEnd();
    }

} // namespace System::detail
