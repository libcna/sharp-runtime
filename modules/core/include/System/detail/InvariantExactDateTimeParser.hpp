// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ArgumentException.hpp"
#include "System/IFormatProvider.hpp"
#include "System/Globalization/DateTimeStyles.hpp"
#include "System/Globalization/DateTimeFormatInfo.hpp"
#include <array>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

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
    /**
     * @brief The month and day names an exact parse matches against, plus the whitespace policy.
     *
     * Added by #2412. The four `k*` tables below stay as the INVARIANT defaults -- they are what a
     * null provider resolves to -- and a caller with a `DateTimeFormatInfo` supplies its names
     * instead. Before this, the scanner could only ever match the invariant names, so a provider
     * handed to `ParseExact` could not have been anything but accepted and ignored.
     *
     * @note **This makes `matchName`'s longest-first rule load-bearing.** #1939 recorded honestly
     * that the rule was *"defensive rather than load-bearing"* for the four invariant tables,
     * because no invariant month or day name is a prefix of another and a mutation taking the
     * first match instead went uncaught. **A caller's names carry no such guarantee** -- "Ma" and
     * "March" are a perfectly legal pair -- so with a provider the rule decides the answer, and
     * #2412 pins it with exactly that shape.
     */
    /**
     * @brief Which token families an exact format may use.
     *
     * Added by #2414. The scanner used to take a `bool forDate` and run one of two blocks, so a
     * format could carry date tokens **or** time tokens and never both -- which is why
     * `DateTime::ParseExact` could not exist at all. The two families are disjoint (`M` is a month
     * and `m` a minute; C++ and .NET are both case-sensitive here), so admitting both is a matter
     * of not rejecting the other family rather than of resolving an ambiguity.
     */
    enum class ExactTokenSet { Date, Time, DateAndTime };

    struct ExactParseOptions {
        std::array<std::string, 12> abbreviatedMonths{};
        std::array<std::string, 12> fullMonths{};
        std::array<std::string, 7>  abbreviatedDays{};
        std::array<std::string, 7>  fullDays{};
        bool allowLeadingWhite = false;
        bool allowTrailingWhite = false;
        bool allowInnerWhite = false;
    };

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
    /** @brief The options a null provider and `DateTimeStyles::None` resolve to. */
    [[nodiscard]] inline ExactParseOptions InvariantExactParseOptions() {
        ExactParseOptions options;
        for (std::size_t i = 0; i < 12; ++i) {
            options.abbreviatedMonths[i] = kAbbreviatedMonths[i];
            options.fullMonths[i] = kFullMonths[i];
        }
        for (std::size_t i = 0; i < 7; ++i) {
            options.abbreviatedDays[i] = kAbbreviatedDays[i];
            options.fullDays[i] = kFullDays[i];
        }
        return options;
    }

    [[nodiscard]] inline std::string ExpandStandardFormat(const std::string& format,
                                                          bool forDate) {
        if (format.size() != 1) return {};
        switch (format[0]) {
            case 'O': case 'o': return forDate ? "yyyy-MM-dd" : "HH:mm:ss.fffffff";
            case 'R': case 'r': return forDate ? "ddd, dd MMM yyyy" : "HH:mm:ss";
            default:            return {};
        }
    }

    /**
     * @brief Expands a one-character standard format for `DateTime` (#2414).
     *
     * SEPARATE FROM `ExpandStandardFormat` BECAUSE THE PATTERNS ARE NOT THE DATE AND TIME HALVES
     * CONCATENATED: .NET's `s` uses a `T` separator where `u` uses a space, and `R` ends in a
     * literal `GMT` that the date-only `R` does not have.
     *
     * TWO OF .NET'S PATTERNS ARE TRANSCRIBED WITH A NAMED LOSS, AND THE LOSS IS THIS PORT'S
     * NO-ZONE-TOKEN BOUNDARY RATHER THAN AN OVERSIGHT:
     *   * `o`/`O` is `yyyy'-'MM'-'dd'T'HH':'mm':'ss'.'fffffffK` in .NET. `K` renders as the empty
     *     string for a `DateTimeKind::Unspecified` value, so the pattern below is exactly .NET's
     *     `o` for that kind and REFUSES the `Z` and `+hh:mm` forms .NET would accept.
     *   * `u` is `yyyy'-'MM'-'dd HH':'mm':'ss'Z'`, whose `Z` is a LITERAL rather than a zone
     *     token -- so it is transcribed in full and merely does not set a kind.
     * Both are pinned, so a later ticket adding a zone token has to revisit them by name.
     */
    [[nodiscard]] inline std::string ExpandStandardDateTimeFormat(const std::string& format) {
        if (format.size() != 1) return {};
        switch (format[0]) {
            case 'O': case 'o': return "yyyy-MM-ddTHH:mm:ss.fffffff";
            case 'R': case 'r': return "ddd, dd MMM yyyy HH:mm:ss 'GMT'";
            case 's':           return "yyyy-MM-ddTHH:mm:ss";
            case 'u':           return "yyyy-MM-dd HH:mm:ss'Z'";
            default:            return {};
        }
    }

    /** @brief A forward cursor over the input, with .NET's digit-width rule. */
    class ExactInputCursor {
    public:
        explicit ExactInputCursor(const std::string& text) noexcept : text_(text) {}
        [[nodiscard]] bool        atEnd() const noexcept { return index_ >= text_.size(); }
        [[nodiscard]] std::size_t position() const noexcept { return index_; }

        /** @brief Consumes any run of whitespace; `DateTimeStyles::AllowInnerWhite` (#2412). */
        void skipWhitespace() noexcept {
            while (index_ < text_.size() &&
                   std::isspace(static_cast<unsigned char>(text_[index_])) != 0) {
                ++index_;
            }
        }

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
        /** @brief #2412 widened this from `const char*`: a provider supplies these names now. */
        bool takeWord(std::string_view word) noexcept {
            if (word.empty()) return false;
            if (index_ + word.size() > text_.size()) return false;
            if (text_.compare(index_, word.size(), word) != 0) return false;
            index_ += word.size();
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
                                               ExactTokenSet tokens, ExactDateTimeFields& fields,
                                               const ExactParseOptions& options) {
        const bool allowDate = tokens != ExactTokenSet::Time;
        const bool allowTime = tokens != ExactTokenSet::Date;
        if (format.empty()) return false;

        // #2412: the whitespace styles. `AllowLeadingWhite`/`AllowTrailingWhite` trim the INPUT
        // before and after matching; `AllowInnerWhite` is handled at each literal, because it
        // permits extra whitespace only where the format already expects a separator -- it does
        // not license whitespace between digits of one field.
        std::string trimmed = input;
        if (options.allowLeadingWhite) {
            const std::size_t first = trimmed.find_first_not_of(" \t\n\v\f\r");
            trimmed = (first == std::string::npos) ? std::string{} : trimmed.substr(first);
        }
        if (options.allowTrailingWhite) {
            const std::size_t last = trimmed.find_last_not_of(" \t\n\v\f\r");
            trimmed = (last == std::string::npos) ? std::string{} : trimmed.substr(0, last + 1);
        }
        ExactInputCursor cursor(trimmed);
        const auto skipInnerWhite = [&cursor, &options] {
            if (options.allowInnerWhite) cursor.skipWhitespace();
        };

        const auto runLength = [&format](std::size_t at) {
            std::size_t n = 1;
            while (at + n < format.size() && format[at + n] == format[at]) ++n;
            return n;
        };
        const auto matchName = [&cursor](const auto& names, int& out) {
            // Longest first, so a name that is a prefix of another cannot win.
            //
            // #1939 recorded honestly that for the four INVARIANT tables this is DEFENSIVE rather
            // than load-bearing -- no invariant month or day name is a prefix of another, and a
            // mutation taking the first match instead went uncaught. #2412 CHANGED THAT: a caller's
            // provider supplies these names now, and a caller's names carry no such guarantee
            // ("Ma" and "March" are a legal pair), so the rule decides the answer and is pinned.
            //
            // The empty-name guards -- this one and `takeWord`'s -- are DEFENCE IN DEPTH and are
            // recorded as such rather than claimed to be tested. DateTimeFormatInfo's month arrays
            // carry an empty thirteenth slot, so the concern is real; but `len <= bestLen` with
            // `bestLen` starting at 0 ALREADY excludes a zero-length name arithmetically, so
            // mutations M7 and M8 remove either guard and neither is caught. Both are kept because
            // the intent should be visible at the point a future reader adds a table.
            int  best = -1;
            std::size_t bestLen = 0;
            for (std::size_t i = 0; i < names.size(); ++i) {
                // #2412 widened `names` from `const char*` tables to std::string, because a
                // provider supplies them; std::string_view spans both without a conversion.
                const std::string_view candidate{names[i]};
                const std::size_t len = candidate.size();
                if (len == 0 || len <= bestLen) continue;
                ExactInputCursor probe = cursor;
                if (probe.takeWord(candidate)) { best = static_cast<int>(i); bestLen = len; }
            }
            if (best < 0) return false;
            (void)cursor.takeWord(std::string_view{names[static_cast<std::size_t>(best)]});
            out = best;
            return true;
        };

        for (std::size_t i = 0; i < format.size();) {
            // #2412: `AllowInnerWhite` skips whitespace AT A TOKEN BOUNDARY -- before each literal
            // and before each field. It is deliberately not "skip whitespace anywhere": a field
            // reader consumes its own digits without ever calling this, so "20 24-06-15" against
            // "yyyy-MM-dd" still fails on the four-digit year, which is the row that separates
            // .NET's rule from a blanket skip.
            skipInnerWhite();
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

            if (allowDate) {
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
                        if (!matchName(n == 3 ? options.abbreviatedMonths : options.fullMonths, index)) return false;
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
                        if (!matchName(n == 3 ? options.abbreviatedDays : options.fullDays, index)) return false;
                        fields.weekday = index;
                    }
                    i += run;
                    continue;
                }
                // A ZONE token is unsupported in every mode: this port's exact grammar carries no
                // zone, which is why RoundtripKind would have nothing to preserve.
                if (c == 'z' || c == 'K' || c == 'g') return false;
                // A time token is unsupported only when time tokens are not admitted at all --
                // otherwise it must FALL THROUGH to the time block below rather than be rejected
                // here, which is the whole of what DateAndTime changes.
                if (!allowTime && (c == 'H' || c == 'h' || c == 'm' || c == 's' || c == 'f' ||
                                   c == 'F' || c == 't'))
                    return false;
            }
            if (allowTime) {
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
                if (c == 'z' || c == 'K' || c == 'g') return false;
                // Mirror of the rule above -- but with an ASYMMETRY that is worth stating, because
                // it was measured rather than assumed. `!allowDate` here is a PROVEN EQUIVALENCE
                // rather than a load-bearing guard: every date-token arm above ends in `continue`,
                // so a date token can never reach this line while the date block is running, and
                // `!allowDate` is exactly "the date block did not run". #2414's mutation M2 drops
                // the condition and no test can see it. The condition is kept for intent -- it
                // says WHY the rejection is correct -- and it becomes load-bearing the day an arm
                // above stops consuming its token.
                if (!allowDate && (c == 'y' || c == 'M' || c == 'd')) return false;
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

namespace System::detail {

    /**
     * @brief `DateOnly`/`TimeOnly`'s whole style contract, which is one line in .NET.
     *
     * `DateOnly.cs:317-320` and `TimeOnly.cs:458,486,520`:
     * `if ((style & ~DateTimeStyles.AllowWhiteSpaces) != 0) throw new ArgumentException(...)`.
     *
     * **Neither type has a `DateTimeKind`**, so every kind-affecting style -- `AdjustToUniversal`,
     * `AssumeLocal`, `AssumeUniversal`, `RoundtripKind` -- and `NoCurrentDateDefault` have nothing
     * to act on, and .NET rejects them rather than ignoring them. That is why this half of #1942
     * needs **no timezone contract**: the styles that would need one are exactly the styles that
     * are illegal here.
     */
    inline void ValidateDateTimeOnlyStyles(System::Globalization::DateTimeStyles style) {
        using System::Globalization::DateTimeStyles;
        const auto outsideWhitespace = static_cast<int>(style) &
                                       ~static_cast<int>(DateTimeStyles::AllowWhiteSpaces);
        if (outsideWhitespace != 0) {
            throw System::ArgumentException(
                "The only allowed values for the styles are AllowWhiteSpaces, AllowTrailingWhite, "
                "AllowLeadingWhite, and AllowInnerWhite.",
                "style");
        }
    }

    /** @brief Builds the scanner's options from a resolved provider and a validated style. */
    inline ExactParseOptions ResolveExactParseOptions(const System::IFormatProvider* provider,
                                                      System::Globalization::DateTimeStyles style) {
        using System::Globalization::DateTimeStyles;
        const auto& info = System::Globalization::DateTimeFormatInfo::GetInstance(provider);
        ExactParseOptions options;
        const auto abbreviatedMonths = info.getAbbreviatedMonthNamesProperty();
        const auto fullMonths = info.getMonthNamesProperty();
        const auto abbreviatedDays = info.getAbbreviatedDayNamesProperty();
        const auto fullDays = info.getDayNamesProperty();
        // DateTimeFormatInfo's month arrays are 0-based with an empty THIRTEENTH slot (.NET's
        // MonthNames convention); the scanner's are twelve, so the empty slot is dropped rather
        // than carried -- a matchName that could match "" would match at every position.
        for (std::size_t i = 0; i < 12; ++i) {
            options.abbreviatedMonths[i] = abbreviatedMonths[i];
            options.fullMonths[i] = fullMonths[i];
        }
        for (std::size_t i = 0; i < 7; ++i) {
            options.abbreviatedDays[i] = abbreviatedDays[i];
            options.fullDays[i] = fullDays[i];
        }
        const auto has = [style](DateTimeStyles bit) {
            return (static_cast<int>(style) & static_cast<int>(bit)) != 0;
        };
        options.allowLeadingWhite = has(DateTimeStyles::AllowLeadingWhite);
        options.allowTrailingWhite = has(DateTimeStyles::AllowTrailingWhite);
        options.allowInnerWhite = has(DateTimeStyles::AllowInnerWhite);
        return options;
    }

} // namespace System::detail
