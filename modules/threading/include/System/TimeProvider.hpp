// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/DateTimeOffset.hpp"
#include "System/DateTime.hpp"
#include "System/TimeSpan.hpp"
#include "System/TimeZoneInfo.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Diagnostics/Stopwatch.hpp"
#include "System/Threading/ITimer.hpp"
#include "System/Threading/Timer.hpp"
#include <limits>
#include <memory>

namespace System {

    using SharpRuntime::longcs;

    /**
     * @brief Provides an abstraction for time.
     * C++ counterpart of .NET System.TimeProvider.
     */
    class TimeProvider {
    public:
        virtual ~TimeProvider() = default;

        /** Returns the system-default TimeProvider (wall clock + high-resolution timer). */
        static TimeProvider& getSystemProperty();

    protected:
        TimeProvider() = default;

    public:
        /** Returns the current UTC date and time. Override to supply a custom clock. */
        virtual DateTimeOffset GetUtcNow() const { return DateTimeOffset::getUtcNowProperty(); }

        /** Returns the current local date and time, converted via LocalTimeZone. */
        DateTimeOffset GetLocalNow() const {
            DateTimeOffset utcNow = GetUtcNow();
            const TimeZoneInfo& zone = getLocalTimeZoneProperty();
            TimeSpan offset = zone.GetUtcOffset(utcNow.getUtcDateTimeProperty());
            if (offset.getTicksProperty() == 0) return utcNow;
            longcs localTicks = utcNow.getUtcTicksProperty() + offset.getTicksProperty();
            constexpr longcs minTicks = 0LL;
            constexpr longcs maxTicks = 3'155'378'975'999'999'999LL;
            if (static_cast<unsigned long long>(localTicks) >
                static_cast<unsigned long long>(maxTicks)) {
                localTicks = localTicks < minTicks ? minTicks : maxTicks;
            }
            return DateTimeOffset(DateTime(localTicks), offset);
        }

        /** Returns the local time zone. Override to supply a fixed zone. */
        virtual const TimeZoneInfo& getLocalTimeZoneProperty() const {
            return TimeZoneInfo::Local();
        }

        /** Returns the frequency of GetTimestamp() in ticks per second. */
        virtual longcs getTimestampFrequencyProperty() const {
            return System::Diagnostics::Stopwatch::Frequency;
        }

        /** Returns the current high-frequency timestamp value. */
        virtual longcs GetTimestamp() const {
            return System::Diagnostics::Stopwatch::GetTimestamp();
        }

        /**
         * Returns elapsed time between two GetTimestamp() values.
         *
         * This body used to carry **two** undefined operations on one line, at two different
         * columns, and only one of them is visible to GCC's default `-fsanitize=undefined` set
         * (SR-AUD-131, ticket #2219; docs/CoreDefinedArithmeticBoundedParseFamilyPlan.md §5.1.1).
         *
         *  1. `endingTimestamp - startingTimestamp` was a signed subtraction. Real .NET evaluates
         *     it in C#'s *unchecked* integral model, where two's-complement wrap is defined and
         *     intended, so the wrap is produced in the unsigned domain and converted back --
         *     CCF-004 class A, changing no value. (CCF-004 stays closed 8/8; this is an
         *     occurrence, not a new member.)
         *  2. The `static_cast<longcs>` of the scaled `double` is undefined whenever the value is
         *     outside `int64`'s range ([conv.fpint]/1), which needs no overflow in step 1 at all:
         *     `GetElapsedTime(0, INT64_MAX)` on the default system provider scales to exactly
         *     2^63 and used to return **INT64_MIN** -- a maximal *negative* duration for a
         *     maximal *positive* interval. It now saturates, which is defined, matches the
         *     saturating floating-to-integer conversion modern .NET adopted, and makes this
         *     method agree with `Stopwatch::GetElapsedTime` on the same arguments.
         */
        TimeSpan GetElapsedTime(longcs startingTimestamp, longcs endingTimestamp) const {
            longcs freq = getTimestampFrequencyProperty();
            if (freq <= 0)
                throw InvalidOperationException("The operation cannot be performed when TimeProvider.TimestampFrequency is zero or negative.");
            const longcs delta = static_cast<longcs>(
                static_cast<SharpRuntime::ulongcs>(endingTimestamp) -
                static_cast<SharpRuntime::ulongcs>(startingTimestamp));
            const double scaled = static_cast<double>(delta) *
                (static_cast<double>(TimeSpan::TicksPerSecond) / static_cast<double>(freq));
            // 2^63 exactly; the representable domain of longcs as a double is [-2^63, 2^63).
            constexpr double twoPow63 = 9223372036854775808.0;
            longcs ticks;
            if (scaled != scaled)                 // NaN: unreachable for a finite delta and a
                ticks = 0;                        // positive finite frequency, guarded anyway
            else if (scaled >= twoPow63)
                ticks = std::numeric_limits<longcs>::max();
            else if (scaled < -twoPow63)
                ticks = std::numeric_limits<longcs>::min();
            else
                ticks = static_cast<longcs>(scaled);
            return TimeSpan(ticks);
        }

        /** Returns elapsed time since startingTimestamp. */
        TimeSpan GetElapsedTime(longcs startingTimestamp) const {
            return GetElapsedTime(startingTimestamp, GetTimestamp());
        }

        /** Creates a new timer that invokes callback after dueTime and then every period. */
        virtual std::unique_ptr<Threading::ITimer> CreateTimer(
            Threading::TimerCallback callback, void* state,
            TimeSpan dueTime, TimeSpan period);
    };

} // namespace System
