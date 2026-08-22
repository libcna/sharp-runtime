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
#include <vector>
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

        // #1942 (SA-16.1/16.3). THIS PORT'S EXACT GRAMMAR CARRIED NO ZONE TOKEN AT ALL until now,
        // which is why #2414 recorded that `RoundtripKind` would have nothing to preserve: an
        // input could never state its own kind. `K` and `z`/`zz`/`zzz` now bind these three.
        bool hasOffset = false;   ///< the input carried a zone token that matched something.
        bool zoneIsUtc = false;   ///< the token was a literal `Z`, .NET's `ParseFlags.TimeZoneUtc`.
        int  offsetMinutes = 0;   ///< signed minutes east of UTC; 0 when `zoneIsUtc`.
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

        /**
         * @brief Whether `K` and `z`/`zz`/`zzz` are admitted (#1942).
         *
         * OFF for `DateOnly` and `TimeOnly`, which have no kind and no offset to carry, and off by
         * default so that admitting a zone is a deliberate act at each door rather than a silent
         * widening of every exact parse in the runtime.
         */
        bool allowZoneToken = false;
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
    /**
     * @brief The ordered first-success loop every multi-format `ParseExact` shares (#1944).
     *
     * ONE DEFINITION FOR FIVE TYPES, because the loop is identical and only the single-format call
     * differs -- five copies of one taxonomy is how five doors come to disagree about what an
     * empty element means.
     *
     * .NET's `TryParseExactMultiple` (`DateTimeParse.cs:170-200`) and
     * `TryParseExactMultipleTimeSpan` (`TimeSpanParse.cs`) are the same four rules in both files:
     *
     *   1. a **null** formats array raises `ArgumentNullException`. **THAT ARM HAS NO C++
     *      COUNTERPART HERE** and is deliberately not reproduced: the parameter is a
     *      `const std::vector<std::string>&`, which cannot be null.
     *   2. an **empty input** is an ordinary parse failure.
     *   3. an **empty formats collection** is a FORMAT failure, not an argument one -- .NET's
     *      `Format_NoFormatSpecifier`. A caller passing no formats gets `FormatException`, which
     *      is easy to get wrong in the direction of `ArgumentException`.
     *   4. an **empty element** ABORTS THE WHOLE LOOP rather than being skipped. This is the
     *      subtle one: "skip a bad format and try the next" is the plausible implementation and it
     *      is wrong, because .NET returns `SetBadFormatSpecifierFailure` immediately.
     *
     * @param tryOne Invoked as `tryOne(format)` and returning whether that format matched; it is
     *        responsible for writing the caller's result on success.
     * @return false for every failure; the caller turns that into its own exception or `false`.
     */
    /**
     * @brief The two failure kinds .NET distinguishes, which this port must distinguish too.
     *
     * MEASURED: with the empty-collection guard simply removed, the loop body never runs and the
     * fall-through `return false` gives the SAME answer -- so the guard would be a proven
     * equivalence and #1944's mutation M2 uncaught. But **.NET's two failures carry DIFFERENT
     * MESSAGES**: `Format_NoFormatSpecifier` is *"No format specifiers were provided."* while an
     * ordinary miss is *"String was not recognized as a valid …"*. Collapsing them would tell a
     * caller who passed no formats that their INPUT was bad, which is the wrong diagnosis.
     */
    enum class MultiFormatOutcome { Matched, NoFormatSpecifier, NotRecognized };

    template <typename TryOne>
    [[nodiscard]] inline MultiFormatOutcome MatchFirstOfManyFormats(
            const std::string& input, const std::vector<std::string>& formats, TryOne&& tryOne) {
        if (formats.empty()) return MultiFormatOutcome::NoFormatSpecifier;
        // A PROVEN EQUIVALENCE, recorded rather than counted (#1944's mutation M3): with this
        // line removed the loop still runs, every format fails to match an empty input, and the
        // fall-through returns the SAME outcome with the SAME message. **.NET's is an equivalence
        // too** -- its `s.Length == 0` arm and its all-formats-failed arm are both
        // `SetBadDateTimeFailure`, one kind and one text -- so this is a statement of intent
        // rather than a rule, kept because it is .NET's and because it says WHY an empty input
        // cannot succeed without relying on every format's scanner to refuse it.
        if (input.empty()) return MultiFormatOutcome::NotRecognized;
        for (const std::string& format : formats) {
            // An empty ELEMENT is `SetBadFormatSpecifierFailure`, which is the same KIND as an
            // empty collection: the caller's formats are wrong, not their input.
            if (format.empty()) return MultiFormatOutcome::NoFormatSpecifier;
            if (tryOne(format)) return MultiFormatOutcome::Matched;
        }
        return MultiFormatOutcome::NotRecognized;
    }

    /**
     * @brief .NET's `DateTimeFormatInfo.ValidateStyles`, transcribed (#1942).
     *
     * THREE RULES, THREE DIFFERENT MESSAGES (`DateTimeFormatInfo.cs:1725-1743`). #2414 transcribed
     * them into its record for whoever took this; this is that transcription made executable.
     *
     * The third rule is written in .NET as an INTEGER comparison on the masked value --
     * `(style & (RoundtripKind | localUniversal | AdjustToUniversal)) > RoundtripKind` -- which
     * does not look like what it means: `RoundtripKind` may not combine with any of the other
     * three. It is spelled out here rather than copied, because a reader cannot check the copy.
     *
     * @param styles The value to validate.
     * @param paramName `"style"` or `"styles"`, which .NET varies BY OVERLOAD rather than using
     *        one name -- so the caller passes its own parameter's name (#2323's rule).
     */
    inline void ValidateDateTimeStyles(System::Globalization::DateTimeStyles styles,
                                       const char* paramName) {
        using System::Globalization::DateTimeStyles;
        constexpr int kValid =
            static_cast<int>(DateTimeStyles::AllowLeadingWhite) |
            static_cast<int>(DateTimeStyles::AllowTrailingWhite) |
            static_cast<int>(DateTimeStyles::AllowInnerWhite) |
            static_cast<int>(DateTimeStyles::NoCurrentDateDefault) |
            static_cast<int>(DateTimeStyles::AdjustToUniversal) |
            static_cast<int>(DateTimeStyles::AssumeLocal) |
            static_cast<int>(DateTimeStyles::AssumeUniversal) |
            static_cast<int>(DateTimeStyles::RoundtripKind);
        const int raw = static_cast<int>(styles);

        if ((raw & ~kValid) != 0) {
            throw System::ArgumentException("An undefined DateTimeStyles value is being used.",
                                            paramName);
        }
        const int assumeBoth = static_cast<int>(DateTimeStyles::AssumeLocal) |
                               static_cast<int>(DateTimeStyles::AssumeUniversal);
        if ((raw & assumeBoth) == assumeBoth) {
            throw System::ArgumentException(
                "The DateTimeStyles values AssumeLocal and AssumeUniversal cannot be used "
                "together.", paramName);
        }
        const int roundtrip = static_cast<int>(DateTimeStyles::RoundtripKind);
        if ((raw & roundtrip) != 0 &&
            (raw & (assumeBoth | static_cast<int>(DateTimeStyles::AdjustToUniversal))) != 0) {
            throw System::ArgumentException(
                "The DateTimeStyles value RoundtripKind cannot be used with the values "
                "AssumeLocal, AssumeUniversal or AdjustToUniversal.", paramName);
        }
    }

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
            // #1942 GAVE `o` ITS `K` BACK. #2414 had to drop it, because the grammar carried no
            // zone token at all and the pattern would not have compiled; the header there named
            // the loss and said a later ticket adding a zone token must revisit this row. This is
            // that ticket, and this is that row -- `o` is now .NET's pattern in full.
            case 'O': case 'o': return "yyyy-MM-ddTHH:mm:ss.fffffffK";
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

        /** @brief The character at the cursor, or NUL at the end. Added by #1942 for the sign. */
        [[nodiscard]] char peek() const noexcept {
            return index_ < text_.size() ? text_[index_] : '\0';
        }

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
                    // The invariant calendar's TwoDigitYearMax is 2049, which is a FIXED value of
                    // the invariant culture rather than culture state -- so `yy` may use it here
                    // even though #1929 declined a two-digit year in the GENERAL parser, where the
                    // width is the discriminator and no format says what was meant.
                    fields.year = (n <= 2 && value <= 99) ? (value <= 49 ? 2000 + value : 1900 + value)
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
                // `g` (the era) is unsupported in every mode -- this port has no era table.
                if (c == 'g') return false;
                // A ZONE token falls through to the shared arm below when the caller admits one,
                // and is otherwise still rejected. Before #1942 it was rejected unconditionally,
                // which is why #2414 recorded that RoundtripKind would have nothing to preserve.
                if (!options.allowZoneToken && (c == 'z' || c == 'K')) return false;
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
                if (c == 'g') return false;
                if (!options.allowZoneToken && (c == 'z' || c == 'K')) return false;
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

            // THE ZONE ARM (#1942), shared rather than duplicated in the two blocks above: a zone
            // belongs to neither family, so putting it in either would have made its admission
            // depend on which block happened to run.
            if (options.allowZoneToken && (c == 'K' || c == 'z')) {
                const int run = static_cast<int>(runLength(i));
                if (c == 'z' && run > 3) return false;
                if (c == 'K' && run > 1) return false;
                if (fields.hasOffset || fields.zoneIsUtc) return false;   // at most once

                // `K` ALONE MATCHES THE EMPTY STRING, and that is .NET's rule rather than
                // leniency: `K` renders empty for an Unspecified kind, so a round trip through
                // `o` must be able to read back what it wrote. `z` has no such form -- it always
                // renders a sign and digits -- so an absent offset fails there.
                if (c == 'K' && cursor.take('Z')) {
                    fields.zoneIsUtc = true;
                    fields.hasOffset = true;
                    i += run;
                    continue;
                }
                const bool signPresent = !cursor.atEnd() &&
                                         (cursor.peek() == '+' || cursor.peek() == '-');
                if (!signPresent) {
                    if (c == 'K') { i += run; continue; }   // Unspecified: nothing to read
                    return false;
                }
                const bool negative = cursor.peek() == '-';
                (void)cursor.take(negative ? '-' : '+');

                int hours = 0;
                // `z` is one or two digits, `zz` and `zzz` exactly two -- .NET's ParseDigits width
                // rule, the same one the date tokens use.
                if (!cursor.takeNumber(c == 'z' && run == 1 ? 1 : 2, hours)) return false;
                int minutes = 0;
                if (c == 'K' || run == 3) {
                    // `zzz` and `K` carry `:mm`; `z` and `zz` do not, which is the whole
                    // difference between them and is easy to collapse into one arm by accident.
                    if (!cursor.take(':')) return false;
                    if (!cursor.takeNumber(2, minutes)) return false;
                }
                if (hours > 14 || minutes > 59) return false;
                fields.hasOffset = true;
                fields.offsetMinutes = (negative ? -1 : 1) * (hours * 60 + minutes);
                i += run;
                continue;
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
