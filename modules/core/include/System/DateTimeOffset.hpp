// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Created by robertvokac on 6/7/25.
//

#pragma once
#include <initializer_list>
#include <vector>
#include "System/IFormatProvider.hpp"
#include "System/Globalization/DateTimeStyles.hpp"

#include <string>

#include "System/Object.hpp"
#include "System/DateTime.hpp"
#include "System/DayOfWeek.hpp"
#include "System/TimeSpan.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    // #1943: a pointer to an incomplete type is all the declarations below need, and a full
    // include here would be circular -- ILocalTimeZone names DateTime.
    class ILocalTimeZone;

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    /**
     * @brief Represents a point in time, typically expressed as a date and time of day,
     * relative to Coordinated Universal Time (UTC).
     *
     * This is a partial C++ counterpart of the .NET System::DateTimeOffset structure.
     * It stores a "clock" DateTime value together with its UTC offset; the UTC instant
     * (used for identity, sorting, and subtraction) is derived as clock ticks minus offset.
     *
     * @note Status: Partial. Deviations from .NET, mirroring the deviations already
     *   documented on System::DateTime:
     *   - DateTimeKind is not tracked. The single-argument DateTime constructor and
     *     ToOffset-free implicit conversion always attach a zero offset instead of
     *     inferring it from Kind.
     *   - ToLocalTime()/LocalDateTime approximate "local" using the current system
     *     UTC offset for every instant (no historical/future DST or timezone-rule
     *     lookup), consistent with System::TimeZoneInfo's own documented limitations.
     *   - Calendar-based constructors, leap-second handling, culture-aware
     *     ToString/Parse (IFormatProvider), span-based TryFormat, Deconstruct, and
     *     FromFileTime/ToFileTime (OLE/FILETIME conversions) are out of scope.
     *   - **`ParseExact`/`TryParseExact` LANDED in #1943 (SA-16.2)** and are no longer part of
     *     that list. Leaving a header describing an absence it no longer has is the SR-AUD-168
     *     defect, so the line is corrected rather than left standing.
     */
    class DateTimeOffset : public Object {
    private:
        DateTime dateTime_;
        TimeSpan offset_;

    public:
        DateTimeOffset();
        DateTimeOffset(const DateTime& dateTime, const TimeSpan& offset);

        /**
         * @brief Initializes a new instance from a DateTime, attaching a zero UTC offset.
         *
         * C++ counterpart of .NET's implicit operator DateTimeOffset(DateTime).
         * @note Since DateTimeKind is not tracked, this always attaches TimeSpan::Zero
         *       rather than inferring the offset from Kind.
         */
        DateTimeOffset(const DateTime& dateTime);

        /**
         * @brief Initializes a new instance from a tick count and a UTC offset.
         *
         * C++ counterpart of .NET DateTimeOffset(long, TimeSpan).
         * @param ticks  Clock time expressed in 100-nanosecond ticks since the .NET epoch.
         * @param offset The time's offset from UTC.
         *
         * @p offset is validated before @p ticks, matching the reference's argument
         * evaluation order.
         *
         * @throws System::ArgumentException if @p offset is not a whole number of minutes.
         * @throws System::ArgumentOutOfRangeException if @p offset is outside ±14 hours, if
         *         @p ticks is outside DateTime's representable range, or if the resulting UTC
         *         time is outside DateTime's representable range.
         */
        DateTimeOffset(longcs ticks, const TimeSpan& offset);

        /**
         * @brief Initializes a new instance from date, time, and offset components.
         *
         * C++ counterpart of .NET DateTimeOffset(int, int, int, int, int, int, TimeSpan).
         *
         * Arguments are validated in the reference's order: @p offset's shape and range
         * first, then the date components, then the time components, then the resulting UTC
         * instant.
         *
         * @throws System::ArgumentException if @p offset is not a whole number of minutes.
         * @throws System::ArgumentOutOfRangeException if @p offset is outside ±14 hours, if
         *         any date or time component is out of range, or if the resulting UTC time is
         *         outside DateTime's representable range.
         */
        DateTimeOffset(intcs year, intcs month, intcs day,
                       intcs hour, intcs minute, intcs second,
                       const TimeSpan& offset);

        /**
         * @brief Initializes a new instance from date, time, millisecond, and offset components.
         *
         * C++ counterpart of .NET DateTimeOffset(int, int, int, int, int, int, int, TimeSpan).
         *
         * Arguments are validated in the reference's order: @p offset's shape and range
         * first, then the date components, then the time components, then @p millisecond,
         * then the resulting UTC instant.
         *
         * @throws System::ArgumentException if @p offset is not a whole number of minutes.
         * @throws System::ArgumentOutOfRangeException if @p offset is outside ±14 hours, if
         *         any date or time component is out of range, or if the resulting UTC time is
         *         outside DateTime's representable range.
         */
        DateTimeOffset(intcs year, intcs month, intcs day,
                       intcs hour, intcs minute, intcs second, intcs millisecond,
                       const TimeSpan& offset);

        /** @brief The smallest possible value of DateTimeOffset (0001-01-01 00:00:00 +00:00). */
        static const DateTimeOffset MinValue;
        /** @brief The largest possible value of DateTimeOffset (9999-12-31 23:59:59.9999999 +00:00). */
        static const DateTimeOffset MaxValue;
        /** @brief Represents the point in time when Unix time begins (1970-01-01 00:00:00 +00:00). */
        static const DateTimeOffset UnixEpoch;

        // Static factory
        /** @brief Gets a DateTimeOffset for the current local date and time, with the local UTC offset. */
        [[nodiscard]] static DateTimeOffset getNowProperty();
        /** @brief Gets a DateTimeOffset for the current UTC date and time, with a zero offset. */
        [[nodiscard]] static DateTimeOffset getUtcNowProperty();

        // Component accessors
        /** @return The DateTime part of this instance, in the instance's own offset. */
        [[nodiscard]] const DateTime& getDateTimeProperty() const;
        /** @return The time's UTC offset. */
        [[nodiscard]] const TimeSpan& getOffsetProperty() const;
        /** @return The UTC offset, in whole minutes. */
        [[nodiscard]] intcs getTotalOffsetMinutesProperty() const;
        /** @return The number of ticks representing the date and time of this instance (offset-local). */
        [[nodiscard]] longcs getTicksProperty() const;
        /** @return The number of ticks representing the date and time of this instance, in UTC. */
        [[nodiscard]] longcs getUtcTicksProperty() const;
        /** @return The year component. */
        [[nodiscard]] intcs getYearProperty()        const;
        /** @return The month component (1-12). */
        [[nodiscard]] intcs getMonthProperty()       const;
        /** @return The day-of-month component. */
        [[nodiscard]] intcs getDayProperty()         const;
        /** @return The day of the week. */
        [[nodiscard]] DayOfWeek getDayOfWeekProperty() const;
        /** @return The day of the year (1-366). */
        [[nodiscard]] intcs getDayOfYearProperty()   const;
        /** @return The hour component (0-23). */
        [[nodiscard]] intcs getHourProperty()        const;
        /** @return The minute component (0-59). */
        [[nodiscard]] intcs getMinuteProperty()      const;
        /** @return The second component (0-59). */
        [[nodiscard]] intcs getSecondProperty()      const;
        /** @return The millisecond component (0-999). */
        [[nodiscard]] intcs getMillisecondProperty() const;
        /** @return The time of day, as a TimeSpan since midnight. */
        [[nodiscard]] TimeSpan getTimeOfDayProperty() const;
        /** @return The date component, with the time set to midnight. */
        [[nodiscard]] DateTime getDateProperty()          const;
        /** @return The UTC DateTime represented by this instance. */
        [[nodiscard]] DateTime getUtcDateTimeProperty()   const;
        /** @return The local DateTime represented by this instance. */
        [[nodiscard]] DateTime getLocalDateTimeProperty() const;

        /**
         * @brief Returns a DateTimeOffset that represents the same point in time as this
         * instance, but with a different UTC offset.
         *
         * C++ counterpart of .NET DateTimeOffset.ToOffset(TimeSpan).
         * @throws System::ArgumentOutOfRangeException if @p offset is outside ±14 hours,
         *         or if the resulting UTC time is outside DateTime's representable range.
         */
        [[nodiscard]] DateTimeOffset ToOffset(const TimeSpan& offset) const;

        // Arithmetic
        /** @brief Returns a new instance offset by the given TimeSpan. */
        [[nodiscard]] DateTimeOffset Add(const TimeSpan& ts) const;
        /** @brief Returns a new instance offset by the given number of days. */
        [[nodiscard]] DateTimeOffset AddDays(double days) const;
        /** @brief Returns a new instance offset by the given number of hours. */
        [[nodiscard]] DateTimeOffset AddHours(double hours) const;
        /** @brief Returns a new instance offset by the given number of minutes. */
        [[nodiscard]] DateTimeOffset AddMinutes(double minutes) const;
        /** @brief Returns a new instance offset by the given number of seconds. */
        [[nodiscard]] DateTimeOffset AddSeconds(double seconds) const;
        /** @brief Returns a new instance offset by the given number of milliseconds. */
        [[nodiscard]] DateTimeOffset AddMilliseconds(double ms) const;
        /** @brief Returns a new instance offset by the given number of ticks. */
        [[nodiscard]] DateTimeOffset AddTicks(longcs ticks) const;
        /** @brief Returns a new instance offset by the given number of months. */
        [[nodiscard]] DateTimeOffset AddMonths(intcs months) const;
        /** @brief Returns a new instance offset by the given number of years. */
        [[nodiscard]] DateTimeOffset AddYears(intcs years) const;
        /** @brief Returns the TimeSpan between this instance and @p other. */
        [[nodiscard]] TimeSpan Subtract(const DateTimeOffset& other) const;
        /** @brief Returns a new instance offset backward by the given TimeSpan. */
        [[nodiscard]] DateTimeOffset Subtract(const TimeSpan& ts) const;
        /** @brief Returns a new instance representing the same point in time, with a zero UTC offset. */
        [[nodiscard]] DateTimeOffset ToUniversalTime() const;

        /**
         * @brief Converts this instance to local time.
         *
         * C++ counterpart of .NET DateTimeOffset.ToLocalTime().
         * @note Approximates "local" using the current system UTC offset (see class note).
         */
        [[nodiscard]] DateTimeOffset ToLocalTime() const;

        /**
         * @brief Converts a Unix time (seconds since 1970-01-01 00:00:00 UTC) to a
         * DateTimeOffset with a zero offset.
         *
         * C++ counterpart of .NET DateTimeOffset.FromUnixTimeSeconds(long).
         * @throws System::ArgumentOutOfRangeException if @p seconds is outside the
         *         representable range.
         */
        [[nodiscard]] static DateTimeOffset FromUnixTimeSeconds(longcs seconds);

        /**
         * @brief Converts a Unix time (milliseconds since 1970-01-01 00:00:00 UTC) to a
         * DateTimeOffset with a zero offset.
         *
         * C++ counterpart of .NET DateTimeOffset.FromUnixTimeMilliseconds(long).
         * @throws System::ArgumentOutOfRangeException if @p milliseconds is outside the
         *         representable range.
         */
        [[nodiscard]] static DateTimeOffset FromUnixTimeMilliseconds(longcs milliseconds);

        /**
         * @brief Returns the number of whole seconds since 1970-01-01 00:00:00 UTC
         * represented by this instance.
         *
         * C++ counterpart of .NET DateTimeOffset.ToUnixTimeSeconds().
         */
        [[nodiscard]] longcs ToUnixTimeSeconds() const;

        /**
         * @brief Returns the number of whole milliseconds since 1970-01-01 00:00:00 UTC
         * represented by this instance.
         *
         * C++ counterpart of .NET DateTimeOffset.ToUnixTimeMilliseconds().
         */
        [[nodiscard]] longcs ToUnixTimeMilliseconds() const;

        // Parsing / formatting
        /** @brief Parses @p s into a DateTimeOffset. @throws System::FormatException if @p s is not a valid date/time. */
        [[nodiscard]] static DateTimeOffset Parse(const std::string& s);
        /** @brief Attempts to parse @p s into @p result. @return false if @p s is not a valid date/time. */
        static bool TryParse(const std::string& s, DateTimeOffset& result);

        /**
         * @brief Parses @p input against exactly @p format.
         *
         * C++ counterpart of .NET `DateTimeOffset.ParseExact(string, string, IFormatProvider,
         * DateTimeStyles)`, added by #1943 under **SA-16.2**.
         *
         * @note **AN OFFSET IS NOT A TIME ZONE, AND THAT IS WHY THIS COULD LAND AT ALL.** A format
         *       carrying an explicit offset token (`zzz` or `K`) needs no zone database whatever:
         *       the offset is read from the input and stored, `DateTimeOffset` being a `DateTime`
         *       plus a `TimeSpan`. Only the **no-offset** case needs a zone, because .NET gives
         *       such a result the **local** offset (`DateTimeOffsetTimeZonePostProcessing`).
         *
         * @note **THE ZONE-LESS OVERLOADS EXIST, AND THE COST IS LARGER HERE THAN FOR `DateTime`.**
         *       There, only a few styles convert; here **every format without an offset token**
         *       needs a zone, so the most ordinary call raises `ArgumentNullException` naming
         *       `zone` unless one is supplied or `AssumeUniversal` is used. SA-16.6 accepted that
         *       knowingly: requiring the zone in the signature would diverge further from .NET,
         *       and refusing a no-offset format would be a narrowing .NET does not have.
         *
         * @throws System::ArgumentException if @p styles is not a legal combination.
         * @throws System::ArgumentNullException if the offset had to be defaulted and @p zone is
         *         null.
         * @throws System::FormatException if @p input does not match @p format.
         */
        [[nodiscard]] static DateTimeOffset ParseExact(const std::string& input,
                                                       const std::string& format);

        /** @brief Parses @p input against exactly @p format, using @p provider's names. */
        [[nodiscard]] static DateTimeOffset ParseExact(const std::string& input,
                                                       const std::string& format,
                                                       const System::IFormatProvider* provider);

        /** @brief Parses @p input against exactly @p format, honouring @p styles. */
        [[nodiscard]] static DateTimeOffset ParseExact(
            const std::string& input, const std::string& format,
            const System::IFormatProvider* provider,
            System::Globalization::DateTimeStyles styles,
            const System::ILocalTimeZone* zone = nullptr);

        /** @brief Non-throwing counterpart of ParseExact(input, format). */
        static bool TryParseExact(const std::string& input, const std::string& format,
                                  DateTimeOffset& result);

        /**
         * @brief Non-throwing counterpart of the style-taking ParseExact.
         *
         * @note **A `Try*` METHOD THAT THROWS** for an illegal style or a missing zone, which is
         *       .NET's own shape: a parse *failure* returns false, a programming error is raised.
         */
        static bool TryParseExact(const std::string& input, const std::string& format,
                                  const System::IFormatProvider* provider,
                                  System::Globalization::DateTimeStyles styles,
                                  DateTimeOffset& result,
                                  const System::ILocalTimeZone* zone = nullptr);

        /**
         * @brief Parses @p input against the FIRST of @p formats that matches (#1944).
         *
         * C++ counterpart of .NET `DateTimeOffset.ParseExact(string, string[], IFormatProvider,
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
        [[nodiscard]] static DateTimeOffset ParseExact(
            const std::string& input, const std::vector<std::string>& formats,
            const System::IFormatProvider* provider = nullptr,
            System::Globalization::DateTimeStyles styles = System::Globalization::DateTimeStyles::None,
            const System::ILocalTimeZone* zone = nullptr);

        /** @brief Non-throwing counterpart of the multi-format ParseExact. */
        static bool TryParseExact(const std::string& input,
                                  const std::vector<std::string>& formats,
                                  const System::IFormatProvider* provider,
                                  System::Globalization::DateTimeStyles styles, DateTimeOffset& result,
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
        [[nodiscard]] static DateTimeOffset ParseExact(
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
                                  System::Globalization::DateTimeStyles styles, DateTimeOffset& result,
                                  const System::ILocalTimeZone* zone = nullptr) {
            return TryParseExact(input, std::vector<std::string>(formats), provider,
                                 styles, result, zone);
        }
        /** @brief Returns a string representation using the default round-trip format. */
        [[nodiscard]] std::string ToString() const override;
        /** @brief Returns a string representation using the given .NET custom/standard format string. */
        [[nodiscard]] std::string ToString(const std::string& format) const;

        using Object::Equals;

        // Comparison
        /** @brief Compares two instances by their point in time. @return negative/zero/positive matching first &lt;/==/&gt; second. */
        [[nodiscard]] static intcs Compare(const DateTimeOffset& first, const DateTimeOffset& second);
        /** @brief Returns true if @p first and @p second represent the same point in time. */
        [[nodiscard]] static bool Equals(const DateTimeOffset& first, const DateTimeOffset& second);
        /** @brief Compares this instance to @p other by their point in time. @return negative/zero/positive matching this &lt;/==/&gt; other. */
        [[nodiscard]] intcs CompareTo(const DateTimeOffset& other) const;
        /** @brief Returns true if @p other represents the same point in time as this instance. */
        [[nodiscard]] bool Equals(const DateTimeOffset& other) const;

        /**
         * @brief Determines whether this instance is equal to @p other, comparing both
         * the UTC instant and the offset.
         *
         * C++ counterpart of .NET DateTimeOffset.EqualsExact(DateTimeOffset).
         */
        [[nodiscard]] bool EqualsExact(const DateTimeOffset& other) const;

        /**
         * @brief Returns a hash code for this DateTimeOffset, based on its UTC instant.
         *
         * C++ counterpart of .NET DateTimeOffset.GetHashCode().
         */
        [[nodiscard]] intcs GetHashCode() const override;

        bool operator==(const DateTimeOffset& other) const;
        bool operator!=(const DateTimeOffset& other) const;
        bool operator< (const DateTimeOffset& other) const;
        bool operator<=(const DateTimeOffset& other) const;
        bool operator> (const DateTimeOffset& other) const;
        bool operator>=(const DateTimeOffset& other) const;

        // Operators
        [[nodiscard]] DateTimeOffset operator+(const TimeSpan& ts) const;
        [[nodiscard]] DateTimeOffset operator-(const TimeSpan& ts) const;
        [[nodiscard]] TimeSpan       operator-(const DateTimeOffset& other) const;

        GetTypeNameHPP()
    };

} // namespace System