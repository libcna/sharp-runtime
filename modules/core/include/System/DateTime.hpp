// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <initializer_list>
#include <vector>

#include <string>
#include "System/DateTimeKind.hpp"

#include "System/Object.hpp"
#include "System/Globalization/DateTimeStyles.hpp"
#include "System/IFormatProvider.hpp"
#include "System/TimeSpan.hpp"
#include "System/DayOfWeek.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::longcs;
    using SharpRuntime::intcs;

    /**
     * @brief Represents an instant in time, expressed as the number of 100-nanosecond
     * ticks since the .NET epoch (0001-01-01 00:00:00).
     *
     * Partial C++ counterpart of .NET System.DateTime.
     *
     * @note Status: Partial — since ticket #1941 (#1929 row 4D, **phase 1**) `DateTimeKind` is
     *   STORED and reported, and `SpecifyKind` and a kind-taking constructor exist. **Since
     *   #1941 phase 2, `ToLocalTime(zone)` and `ToUniversalTime(zone)` convert by it** -- taking
     *   the zone explicitly, because `Core.Base` cannot name one; the no-argument forms .NET has
     *   remain absent and that deviation is recorded on those members. Still absent: offset/`Z`
     *   parse conversion and the `AssumeLocal`/`AssumeUniversal`/`AdjustToUniversal`/
     *   `RoundtripKind` styles. A phase-2 approval must name a date-sensitive timezone provider
     *   before any of those can exist. OLE Automation date, FILETIME, and binary-serialization conversions
     *   (ToOADate/FromOADate, ToFileTime/FromFileTime, ToBinary/FromBinary) are out of
     *   scope. **Since #1940 a culture-aware `ToString(format, IFormatProvider*)` exists** and
     *   honours the resolved `DateTimeFormatInfo`'s month and day names; `Parse`/`TryParse` still
     *   take no provider, deliberately, because this port's general date parser reads no
     *   culture-driven token, so a provider overload there could only accept and ignore one --
     *   which #1940's own acceptance criterion forbids. Widening the parser is #1942.
     *   `ToString(format)` and the parsers use invariant numeric tokens only.
     */
    class ILocalTimeZone;

    class DateTime : public Object {
    public:
        static constexpr longcs TicksPerMillisecond = 10000LL;
        static constexpr longcs TicksPerSecond      = 10000000LL;
        static constexpr longcs TicksPerMinute      = 600000000LL;
        static constexpr longcs TicksPerHour        = 36000000000LL;
        static constexpr longcs TicksPerDay         = 864000000000LL;
        /** @brief Ticks from the .NET epoch (0001-01-01) to the Unix epoch (1970-01-01). */
        static constexpr longcs UnixEpochTicks      = 621355968000000000LL;
        /** @brief The maximum tick value representable (9999-12-31 23:59:59.9999999). */
        static constexpr longcs MaxTicks            = 3155378975999999999LL;

    private:
        // Matches real .NET's private MaxDays/MaxHours constants (DateTime.cs: `MaxTicks /
        // TimeSpan.TicksPerDay` etc.) -- AddDays/AddHours must reject a unit count whose
        // product with TicksPer{Day,Hour} would itself overflow int64 *before* multiplying,
        // since `static_cast<longcs>(days) * TicksPerDay` for a merely large (not even
        // extreme) `int` day count is real signed-overflow UB in C++ otherwise (confirmed via
        // UBSan: 1e9 days or 1e9 hours both overflow). AddMinutes/AddSeconds/AddMilliseconds
        // don't need an equivalent guard: their own MaxMinutes/MaxSeconds/MaxMilliseconds
        // bounds exceed intcs's range, so no representable int argument can reach the
        // multiplication-overflow threshold for those units.
        static constexpr longcs MaxDays  = MaxTicks / TicksPerDay;
        static constexpr longcs MaxHours = MaxTicks / TicksPerHour;

        /**
         * @brief The tick count with the kind packed into its two most significant bits.
         *
         * Ticket #1941 (#1929 row 4D, phase 1). .NET packs `DateTimeKind` into the top two bits
         * of an unsigned 64-bit `_dateData` (`DateTime.cs:118-123,137`), which is what keeps
         * `sizeof(DateTime)` unchanged: `MaxTicks` is `0x2BCA2875F4373FFF`, so 62 bits carry
         * every representable value and two are free.
         *
         *     TicksMask  0x3FFFFFFFFFFFFFFF
         *     FlagsMask  0xC000000000000000
         *     Utc        0x4000000000000000
         *     Local      0x8000000000000000
         *     LocalAmbiguousDst 0xC000000000000000
         *
         * **The member is renamed, and that is the safety property.** A bare `ticks()` used to
         * mean "the tick count"; after packing it would silently mean "the tick count with two
         * flag bits on top", and every one of the thirty reads in this type would have had to be
         * remembered. Renaming makes the compiler visit each one, and the masked value is
         * reachable only through `ticks()`.
         */
        unsigned long long dateData_;

        /** @brief The tick count, with the kind bits removed. .NET's `UTicks`. */
        [[nodiscard]] constexpr longcs ticks() const noexcept {
            return static_cast<longcs>(dateData_ & 0x3FFFFFFFFFFFFFFFULL);
        }

        /**
         * @brief Decomposes ticks() into a UTC std::tm using the C standard library.
         *
         * Uses floor division so that pre-1970 (negative Unix-timestamp) dates are
         * decomposed correctly.
         */
        [[nodiscard]] std::tm toTm() const;

        /**
         * @brief Converts a calendar date and time to a tick count measured from the
         * .NET epoch (0001-01-01 00:00:00).
         *
         * @param year         Year (1–9999).
         * @param month        Month (1–12).
         * @param day          Day of month (1–31, validated against the given month).
         * @param hour         Hour (0–23). Default 0.
         * @param minute       Minute (0–59). Default 0.
         * @param second       Second (0–59). Default 0.
         * @param millisecond  Millisecond (0–999). Default 0.
         * @throws System::ArgumentOutOfRangeException if any component is out of the valid
         *         range. Components are checked in .NET's order — year/month/day first, then
         *         hour/minute/second, then millisecond — and every check happens before any
         *         arithmetic, so the returned tick count is always within [0, MaxTicks].
         */
        static longcs dateToTicks(int year, int month, int day,
                                   int hour = 0, int minute = 0,
                                   int second = 0, int millisecond = 0);

    public:
        /**
         * @brief Initializes a new instance with zero ticks (0001-01-01 00:00:00).
         */
        DateTime();

        /**
         * @brief Initializes a new instance with the specified number of ticks.
         *
         * @param ticks A date and time expressed in 100-nanosecond ticks since
         *              the .NET epoch (0001-01-01 00:00:00).
         * @throws System::ArgumentOutOfRangeException if @p ticks is negative or greater than MaxTicks.
         */
        explicit DateTime(longcs ticks);

        /**
         * @brief Initializes a new instance with the specified ticks and `DateTimeKind`.
         *
         * Ticket #1941 (#1929 row 4D, **phase 1 only**). The kind is *stored and reported*;
         * nothing converts by it. `ToLocalTime`, `ToUniversalTime`, offset/`Z` parse conversion,
         * `AssumeLocal`, `AssumeUniversal`, `AdjustToUniversal` and `RoundtripKind` are all
         * explicitly outside this phase and remain absent — a phase-2 approval must name a
         * date-sensitive timezone provider first.
         *
         * @param ticks A date and time expressed in 100-nanosecond ticks since 0001-01-01.
         * @param kind  Whether @p ticks is UTC, local, or unspecified.
         * @throws System::ArgumentOutOfRangeException if @p ticks is out of range.
         * @throws System::ArgumentException if @p kind is not a declared `DateTimeKind` value.
         *         .NET's test is `(uint)kind > (uint)DateTimeKind.Local` with
         *         `SR.Argument_InvalidDateTimeKind` (`DateTime.cs:206,1309`).
         */
        DateTime(longcs ticks, DateTimeKind kind);

        /**
         * @brief Initializes a new instance with the specified year, month, and day.
         *
         * @param year   Year (1–9999).
         * @param month  Month (1–12).
         * @param day    Day of month (1–max for the given month/year).
         * @throws std::out_of_range if any component is out of range.
         */
        DateTime(intcs year, intcs month, intcs day);

        /**
         * @brief Initializes a new instance with date and time components.
         *
         * @param year    Year (1–9999).
         * @param month   Month (1–12).
         * @param day     Day of month.
         * @param hour    Hour (0–23).
         * @param minute  Minute (0–59).
         * @param second  Second (0–59).
         * @throws System::ArgumentOutOfRangeException if any component is out of range. An
         *         out-of-range hour, minute or second is rejected, never normalized into a
         *         neighbouring instant.
         */
        DateTime(intcs year, intcs month, intcs day, intcs hour, intcs minute, intcs second);

        /**
         * @brief Initializes a new instance with date, time, and millisecond components.
         *
         * @param year         Year (1–9999).
         * @param month        Month (1–12).
         * @param day          Day of month.
         * @param hour         Hour (0–23).
         * @param minute       Minute (0–59).
         * @param second       Second (0–59).
         * @param millisecond  Millisecond (0–999).
         * @throws System::ArgumentOutOfRangeException if any component is out of range. An
         *         out-of-range hour, minute, second or millisecond is rejected, never
         *         normalized into a neighbouring instant.
         */
        DateTime(intcs year, intcs month, intcs day,
                 intcs hour, intcs minute, intcs second, intcs millisecond);

        /** @brief The smallest possible value of DateTime (0001-01-01 00:00:00). */
        static const DateTime MinValue;
        /** @brief The largest possible value of DateTime (9999-12-31 23:59:59.9999999). */
        static const DateTime MaxValue;
        /** @brief Represents the point in time when Unix time begins (1970-01-01 00:00:00 UTC). */
        static const DateTime UnixEpoch;

        /**
         * @brief Gets the number of 100-nanosecond ticks since the .NET epoch.
         *
         * @return Tick count (0 = 0001-01-01 00:00:00).
         */
        [[nodiscard]] longcs getTicksProperty() const;

        /**
         * @brief Gets whether this instance is UTC, local, or neither.
         *
         * Ticket #1941. Every constructor that does not take a kind produces `Unspecified`,
         * which is .NET's default and this port's previous universal behaviour, so no existing
         * value moved.
         *
         * .NET folds its fourth encoding — `LocalAmbiguousDst`, the marker a local time inside a
         * repeated DST hour carries — onto `Local` here, with a bit trick whose comment explains
         * it: *"values 0-2 map directly to DateTimeKind, 3 (LocalAmbiguousDst) needs to be mapped
         * to 2 (Local)"* (`DateTime.cs:1458-1467`). The encoding reserves that fourth value even
         * though nothing in this phase sets it, because reserving it now is what lets phase 2 add
         * ambiguous-local handling without moving any bit.
         */
        [[nodiscard]] DateTimeKind getKindProperty() const;

        /**
         * @brief Returns a copy of @p value with the specified kind and the same ticks.
         *
         * C++ counterpart of .NET `DateTime.SpecifyKind` (`DateTime.cs:1307-1311`). It converts
         * nothing: `SpecifyKind(t, Utc).getTicksProperty() == t.getTicksProperty()` always.
         *
         * @throws System::ArgumentException if @p kind is not a declared `DateTimeKind` value.
         */
        [[nodiscard]] static DateTime SpecifyKind(const DateTime& value, DateTimeKind kind);

        /**
         * @brief Converts to local time against @p zone. `DateTime.cs:1705-1722`.
         *
         * Added by #1941 **phase 2** under SA-15.1.
         *
         * @note **The no-argument `ToLocalTime()` .NET has is still absent, and that is the
         * deviation SA-15.1 accepted rather than an omission.** .NET's takes no parameter because
         * it reaches `TimeZoneInfo.Local` directly; `DateTime` lives in `Core.Base` and every
         * timezone type lives in `TimeZone`, which depends on `Core.Base`, so there is nothing
         * here for a no-argument form to ask. The two ways out were a **registration hook** --
         * hidden global state with a static-initialisation-order dependency -- and this: an
         * overload that takes the source explicitly. It is the same answer #1940 gave for format
         * providers, where `GetInstance(nullptr)` resolves to the invariant info and a caller who
         * wants the current culture **passes it**. Callers write
         * `dt.ToLocalTime(System::TimeZone::CurrentTimeZone())`.
         *
         * @note **`Unspecified` is treated as UTC here**, which is .NET's: `:1707` tests only the
         * `Local` bit, so anything not already local is converted as though it were UTC. The
         * mirror rule is on `ToUniversalTime`, and the two are deliberately not symmetric.
         *
         * @note **Overflow clamps rather than throws** (`:1721`), and the result's `Kind` is
         * `Local`. The reserved `LocalAmbiguousDst` encoding phase 1 transcribed stays
         * unreachable: producing it needs an *ambiguity* answer, and this port's
         * `TimeZoneInfo::IsAmbiguousTime` is documented as always `false`, so inventing one here
         * would be fabricating a distinction the runtime cannot make.
         */
        [[nodiscard]] DateTime ToLocalTime(const ILocalTimeZone& zone) const;

        /**
         * @brief Converts to UTC against @p zone. `DateTime.cs:1771-1772`.
         *
         * @note **`Unspecified` is treated as LOCAL here** -- the mirror of `ToLocalTime`'s rule
         * and, like it, .NET's: `:1772` returns early only for `Utc`. The asymmetry is the point,
         * so each direction has its own pin.
         */
        [[nodiscard]] DateTime ToUniversalTime(const ILocalTimeZone& zone) const;

        /**
         * @brief Gets the year component of this instance (1–9999).
         */
        [[nodiscard]] intcs getYearProperty() const;

        /**
         * @brief Gets the month component of this instance (1–12).
         */
        [[nodiscard]] intcs getMonthProperty() const;

        /**
         * @brief Gets the day-of-month component of this instance (1–31).
         */
        [[nodiscard]] intcs getDayProperty() const;

        /**
         * @brief Gets the hour component of this instance (0–23).
         */
        [[nodiscard]] intcs getHourProperty() const;

        /**
         * @brief Gets the minute component of this instance (0–59).
         */
        [[nodiscard]] intcs getMinuteProperty() const;

        /**
         * @brief Gets the second component of this instance (0–59).
         */
        [[nodiscard]] intcs getSecondProperty() const;

        /**
         * @brief Gets the millisecond component of this instance (0–999).
         */
        [[nodiscard]] intcs getMillisecondProperty() const;

        /**
         * @brief Gets the day of the week represented by this instance.
         *
         * @return DayOfWeek enumeration value (Sunday = 0, …, Saturday = 6).
         */
        [[nodiscard]] DayOfWeek getDayOfWeekProperty() const;

        /**
         * @brief Gets the day of the year represented by this instance (1–366).
         */
        [[nodiscard]] intcs getDayOfYearProperty() const;

        /**
         * @brief Adds the specified time span to this instance.
         *
         * @param value Time span to add.
         * @return A new DateTime that is the sum of this instance and @p value.
         */
        [[nodiscard]] DateTime Add(const TimeSpan& value) const;

        /**
         * @brief Returns a new DateTime with the specified number of whole days added.
         *
         * @param days Number of days to add (may be negative).
         * @return A new DateTime.
         */
        [[nodiscard]] DateTime AddDays(intcs days) const;

        /**
         * @brief Returns a new DateTime with the specified number of hours added.
         *
         * @param hours Number of hours to add (may be negative).
         * @return A new DateTime.
         */
        [[nodiscard]] DateTime AddHours(intcs hours) const;

        /**
         * @brief Returns a new DateTime with the specified number of minutes added.
         *
         * @param minutes Number of minutes to add (may be negative).
         * @return A new DateTime.
         */
        [[nodiscard]] DateTime AddMinutes(intcs minutes) const;

        /**
         * @brief Returns a new DateTime with the specified number of seconds added.
         *
         * @param seconds Number of seconds to add (may be negative).
         * @return A new DateTime.
         */
        [[nodiscard]] DateTime AddSeconds(intcs seconds) const;

        /**
         * @brief Returns a new DateTime with the specified number of milliseconds added.
         *
         * @param milliseconds Number of milliseconds to add (may be negative).
         * @return A new DateTime.
         */
        [[nodiscard]] DateTime AddMilliseconds(intcs milliseconds) const;

        /**
         * @brief Returns a new DateTime with the specified number of 100-nanosecond ticks added.
         *
         * @param value Number of ticks to add (may be negative).
         * @return A new DateTime.
         * @throws std::out_of_range if the result is outside the representable range.
         */
        [[nodiscard]] DateTime AddTicks(longcs value) const;

        /**
         * @brief Returns a new DateTime with the specified number of months added.
         *
         * If the resulting day would be invalid for the resulting month, the day is
         * clamped to the last valid day of that month (e.g. Jan 31 + 1 month = Feb 28/29).
         *
         * @param months Number of months to add (may be negative; must be in [-120000, 120000]).
         * @return A new DateTime.
         * @throws std::out_of_range if @p months is out of range or the resulting year is outside [1, 9999].
         */
        [[nodiscard]] DateTime AddMonths(intcs months) const;

        /**
         * @brief Returns a new DateTime with the specified number of years added.
         *
         * If the current date is February 29 and the resulting year is not a leap year,
         * the result is clamped to February 28.
         *
         * @param value Number of years to add (may be negative; must be in [-10000, 10000]).
         * @return A new DateTime.
         * @throws std::out_of_range if @p value is out of range or the resulting year is outside [1, 9999].
         */
        [[nodiscard]] DateTime AddYears(intcs value) const;

        /**
         * @brief Subtracts the specified time span from this instance.
         *
         * @param value Time span to subtract.
         * @return A new DateTime that is this instance minus @p value.
         */
        [[nodiscard]] DateTime Subtract(const TimeSpan& value) const;

        /**
         * @brief Subtracts another DateTime from this instance.
         *
         * @param value The DateTime to subtract.
         * @return A TimeSpan representing the interval between the two values.
         */
        [[nodiscard]] TimeSpan Subtract(const DateTime& value) const;

        /**
         * @brief Gets the current local date and time.
         *
         * @return Current local DateTime expressed in .NET-compatible ticks.
         * @note DateTimeKind is not stored; the value reflects UTC-based system time.
         */
        [[nodiscard]] static DateTime getNowProperty();

        /**
         * @brief Gets the current date with the time component set to midnight (00:00:00).
         *
         * @return Today's date at 00:00:00.
         */
        [[nodiscard]] static DateTime getTodayProperty();

        /**
         * @brief Gets the time-of-day component of this instance as a TimeSpan.
         *
         * @return A TimeSpan representing the time elapsed since midnight.
         */
        [[nodiscard]] TimeSpan getTimeOfDayProperty() const;

        /**
         * @brief Returns an ISO-8601-style string representation: "YYYY-MM-DD HH:MM:SS".
         *
         * @return Formatted date/time string.
         */
        [[nodiscard]] std::string ToString() const override;

        /**
         * @brief Returns the date/time formatted according to @p format.
         *
         * C++ counterpart of .NET DateTime.ToString(string).
         * Tokens: yyyy, yy, MMMM, MMM, MM, M, dddd, ddd, dd, d, HH, H, hh, h, mm, m, ss, s, fff,
         * ff, f. ddd/dddd and MMM/MMMM use fixed invariant-culture English names (no locale
         * support). Literal text can be enclosed in single quotes.
         * @param format The format string.
         */
        [[nodiscard]] std::string ToString(const std::string& format) const;

        /**
         * @brief Formats using the month and day names the provider supplies.
         *
         * C++ counterpart of .NET `DateTime.ToString(string, IFormatProvider)`. Added by #1940
         * (SA-14 decision 1), which is the ticket that made a provider REACHABLE from here:
         * `DateTimeFormatInfo` moved into `Core.Base`, and it and `CultureInfo` gained the
         * `IFormatProvider` implementations this runtime had none of.
         *
         * The provider is resolved by `DateTimeFormatInfo::GetInstance` -- null means the current
         * info, a `DateTimeFormatInfo` is itself, anything else is asked through `GetFormat` --
         * and the resolved info's `MonthNames`, `AbbreviatedMonthNames`, `DayNames` and
         * `AbbreviatedDayNames` are what `MMMM`, `MMM`, `dddd` and `ddd` emit.
         *
         * @note **The provider is honoured, not accepted and ignored**, which is #1940's own
         * acceptance criterion: hand it an info with different month names and the output moves.
         * @note **And no existing result changes.** The one-argument overload delegates here with
         * a null provider, which resolves to the invariant info, whose names are byte-for-byte
         * the tables this method used to hard-code -- so `ToString(format)` is unchanged, which is
         * the other half of the criterion ("keep all parsers semantically unchanged").
         * @note Numeric tokens, separators and quoting are untouched: they are not culture-driven
         * in this port, and widening them is #1942's subject rather than this ticket's.
         */
        [[nodiscard]] std::string ToString(const std::string& format,
                                            const System::IFormatProvider* provider) const;

        /**
         * @brief Parses a date/time string in ISO-8601 style.
         *
         * C++ counterpart of .NET DateTime.Parse(string).
         * Accepts "yyyy-MM-dd", "yyyy-MM-dd HH:mm:ss", "yyyy-MM-ddTHH:mm:ss",
         * or with optional ".fff" millisecond suffix.
         * @throws System::FormatException if the string cannot be parsed.
         */
        [[nodiscard]] static DateTime Parse(const std::string& s);

        /**
         * @brief Tries to parse a date/time string; returns false without throwing on failure.
         *
         * C++ counterpart of .NET DateTime.TryParse(string, out DateTime).
         */
        static bool TryParse(const std::string& s, DateTime& result);

        /**
         * @brief Parses @p input against exactly @p format, in the invariant culture.
         *
         * C++ counterpart of .NET `DateTime.ParseExact(string, string, IFormatProvider)`.
         *
         * ADDED BY #2414, AND ITS ABSENCE WAS THE REASON #1942 HAD NOWHERE TO LAND: this type's
         * entire parse surface was `Parse` and `TryParse`, so there was no exact-parsing member
         * for a format provider or a `DateTimeStyles` to reach -- the same cycle #2412 resolved
         * for `DateOnly` and `TimeOnly` one type over.
         *
         * @note **#1942 (SA-16.1) ADDED THE STYLE-TAKING OVERLOADS**, which #2414 left absent. The
         *       paragraph below is kept because it records WHY they could not land then, and the
         *       answer -- the zone is a parameter -- is the one it predicted.
         *       `DateTimeStyles`'s kind-affecting members need a local time zone -- `AssumeLocal`
         *       and `AdjustToUniversal` CONVERT, and .NET reaches `TimeZoneInfo.Local` internally
         *       where `Core.Base` cannot. #1941 phase 2 resolved that one level down by TAKING THE
         *       ZONE AS A PARAMETER, so the styles overload needs the same decision made about its
         *       signature; it is #1942's, not this ticket's, and is recorded there rather than
         *       guessed at here. `RoundtripKind` additionally has nothing to preserve, because
         *       this port's exact grammar carries NO ZONE TOKEN at all (`z`, `K` and `g` are
         *       rejected in every mode), so an input can never state its own kind.
         *
         * @throws System::FormatException if @p input does not match @p format.
         */
        [[nodiscard]] static DateTime ParseExact(const std::string& input, const std::string& format);

        /**
         * @brief Parses @p input against exactly @p format, using @p provider's names.
         *
         * A null @p provider means the invariant culture, which is what .NET's own null means.
         */
        [[nodiscard]] static DateTime ParseExact(const std::string& input, const std::string& format,
                                                 const System::IFormatProvider* provider);

        /** @brief Non-throwing counterpart of ParseExact(input, format). */
        static bool TryParseExact(const std::string& input, const std::string& format,
                                  DateTime& result);

        /** @brief Non-throwing counterpart of ParseExact(input, format, provider). */
        static bool TryParseExact(const std::string& input, const std::string& format,
                                  const System::IFormatProvider* provider, DateTime& result);

        /**
         * @brief Parses @p input against exactly @p format, honouring @p styles.
         *
         * C++ counterpart of .NET `DateTime.ParseExact(string, string, IFormatProvider,
         * DateTimeStyles)`, added by #1942 under **SA-16.1**.
         *
         * @note **THE ZONE IS A PARAMETER, AND THAT IS THE ONE DEVIATION FROM .NET'S SIGNATURE.**
         *       `AdjustToUniversal` and `AssumeLocal` **convert** rather than merely stamping a
         *       kind, and a conversion needs a local zone. .NET reaches `TimeZoneInfo.Local`
         *       internally; `Core.Base` cannot name a time zone at all, so the caller supplies one
         *       -- normally `System::TimeZone::CurrentTimeZone()`. This is #1941 phase 2's own
         *       shape (`ToLocalTime(zone)`), so the port answers the question once rather than
         *       twice, and SA-16.1 records that both alternatives were declined: accepting only
         *       the *stamping* styles would make a legal .NET style illegal here, and a
         *       registration hook is the hidden global state #1940 already refused.
         *
         * @note **A NULL @p zone IS ONLY AN ERROR WHEN A STYLE ACTUALLY NEEDS ONE.** The default
         *       argument therefore costs nothing: every style that does not convert works without
         *       a zone, and one that does raises `ArgumentNullException` naming `zone` rather than
         *       silently producing an unconverted value.
         *
         * @throws System::ArgumentException if @p styles is not a legal combination.
         * @throws System::ArgumentNullException if a converting style was used with a null zone.
         * @throws System::FormatException if @p input does not match @p format.
         */
        [[nodiscard]] static DateTime ParseExact(
            const std::string& input, const std::string& format,
            const System::IFormatProvider* provider,
            System::Globalization::DateTimeStyles styles,
            const System::ILocalTimeZone* zone = nullptr);

        /**
         * @brief Non-throwing counterpart of the style-taking ParseExact.
         *
         * @note **A `Try*` METHOD THAT THROWS**, which is .NET's own shape: a parse *failure*
         *       returns false, but an illegal style or a missing zone is a programming error and
         *       is raised. Validation runs BEFORE @p result is written.
         */
        static bool TryParseExact(const std::string& input, const std::string& format,
                                  const System::IFormatProvider* provider,
                                  System::Globalization::DateTimeStyles styles,
                                  DateTime& result,
                                  const System::ILocalTimeZone* zone = nullptr);

        /**
         * @brief Parses @p input against the FIRST of @p formats that matches (#1944).
         *
         * C++ counterpart of .NET `DateTime.ParseExact(string, string[], IFormatProvider,
         * DateTimeStyles)`.
         *
         * @note **AN EMPTY ELEMENT ABORTS THE WHOLE LOOP RATHER THAN BEING SKIPPED**, which is the
         *       rule a plausible implementation gets wrong: .NET returns its bad-format-specifier
         *       failure immediately rather than trying the next entry.
         *
         * @note **AN EMPTY COLLECTION IS A FORMAT FAILURE, NOT AN ARGUMENT ONE** -- .NET's
         *       `Format_NoFormatSpecifier` -- so a caller passing no formats gets
         *       `FormatException`, which is easy to get wrong in the direction of
         *       `ArgumentException`.
         *
         * @note .NET's **null formats array** raises `ArgumentNullException`; that arm has **no
         *       C++ counterpart here** and is deliberately not reproduced, the parameter being a
         *       `const std::vector<std::string>&` that cannot be null.
         */
        [[nodiscard]] static DateTime ParseExact(
            const std::string& input, const std::vector<std::string>& formats,
            const System::IFormatProvider* provider = nullptr,
            System::Globalization::DateTimeStyles styles = System::Globalization::DateTimeStyles::None,
            const System::ILocalTimeZone* zone = nullptr);

        /** @brief Non-throwing counterpart of the multi-format ParseExact. */
        static bool TryParseExact(const std::string& input,
                                  const std::vector<std::string>& formats,
                                  const System::IFormatProvider* provider,
                                  System::Globalization::DateTimeStyles styles, DateTime& result,
                                  const System::ILocalTimeZone* zone = nullptr);

        /**
         * @brief `std::initializer_list` overload of the multi-format ParseExact (#1944).
         *
         * **THIS EXISTS TO KEEP A BRACED LIST OUT OF `std::string`'s ITERATOR-PAIR CONSTRUCTOR,
         * and the wrong branch is undefined behaviour rather than merely a wrong overload.**
         * Measured: without it, `ParseExact(s, {"a", "b"})` is **ambiguous** between the
         * single-format `(string, string)` -- where two `const char*` in braces match
         * `basic_string(InputIt first, InputIt last)` over two **unrelated** pointers -- and the
         * multi-format one. `{"one"}` is ambiguous too; three or more elements are not, and an
         * explicit `std::vector<std::string>{...}` never was.
         *
         * A braced list binds to an `initializer_list` parameter by a **list-initialization
         * sequence**, which outranks any user-defined conversion, so this overload takes it
         * unambiguously and the dangerous candidate can no longer win.
         */
        [[nodiscard]] static DateTime ParseExact(
            const std::string& input, std::initializer_list<std::string> formats,
            const System::IFormatProvider* provider = nullptr,
            System::Globalization::DateTimeStyles styles = System::Globalization::DateTimeStyles::None,
            const System::ILocalTimeZone* zone = nullptr) {
            return ParseExact(input, std::vector<std::string>(formats), provider,
                              styles, zone);
        }

        /** @brief `std::initializer_list` overload of the multi-format TryParseExact. */
        static bool TryParseExact(const std::string& input,
                                  std::initializer_list<std::string> formats,
                                  const System::IFormatProvider* provider,
                                  System::Globalization::DateTimeStyles styles, DateTime& result,
                                  const System::ILocalTimeZone* zone = nullptr) {
            return TryParseExact(input, std::vector<std::string>(formats), provider,
                                 styles, result, zone);
        }

        using Object::Equals;

        /**
         * @brief Determines whether this instance is equal to @p value.
         *
         * C++ counterpart of .NET DateTime.Equals(DateTime).
         */
        [[nodiscard]] bool Equals(const DateTime& value) const { return *this == value; }

        /**
         * @brief Compares this instance to a specified DateTime value.
         *
         * C++ counterpart of .NET DateTime.CompareTo(DateTime).
         * @return Less than zero if this instance is earlier than @p value, zero if equal,
         *         greater than zero if this instance is later than @p value.
         */
        [[nodiscard]] intcs CompareTo(const DateTime& value) const {
            if (ticks() > value.ticks()) return 1;
            if (ticks() < value.ticks()) return -1;
            return 0;
        }

        /**
         * @brief Returns a hash code for this DateTime.
         *
         * C++ counterpart of .NET DateTime.GetHashCode().
         */
        [[nodiscard]] intcs GetHashCode() const override {
            return static_cast<intcs>(ticks()) ^ static_cast<intcs>(ticks() >> 32);
        }

        /**
         * @brief Determines whether the specified year is a leap year.
         *
         * C++ counterpart of .NET DateTime.IsLeapYear(int).
         * @throws std::out_of_range if @p year is outside [1, 9999].
         */
        [[nodiscard]] static bool IsLeapYear(intcs year);

        /**
         * @brief Returns the number of days in the specified month and year.
         *
         * C++ counterpart of .NET DateTime.DaysInMonth(int, int).
         * @throws std::out_of_range if @p month is outside [1, 12] or @p year is outside [1, 9999].
         */
        [[nodiscard]] static intcs DaysInMonth(intcs year, intcs month);

        [[nodiscard]] bool operator==(const DateTime& other) const;
        [[nodiscard]] bool operator!=(const DateTime& other) const;
        [[nodiscard]] bool operator<(const DateTime& other)  const;
        [[nodiscard]] bool operator<=(const DateTime& other) const;
        [[nodiscard]] bool operator>(const DateTime& other)  const;
        [[nodiscard]] bool operator>=(const DateTime& other) const;

        /** @brief Returns a new DateTime that is this instance plus @p value. */
        [[nodiscard]] DateTime operator+(const TimeSpan& value) const { return Add(value); }
        /** @brief Returns a new DateTime that is this instance minus @p value. */
        [[nodiscard]] DateTime operator-(const TimeSpan& value) const { return Subtract(value); }
        /** @brief Returns the TimeSpan elapsed between @p other and this instance. */
        [[nodiscard]] TimeSpan operator-(const DateTime& other) const { return Subtract(other); }

        GetTypeNameHPP()
    };

} // namespace System
