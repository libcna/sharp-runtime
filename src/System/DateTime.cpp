// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include "System/DateTime.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace System {

    // -------------------------------------------------------------------------
    // Private helpers
    // -------------------------------------------------------------------------

    // Converts ticks_ to a UTC std::tm via the C library.
    // Uses int64 time_t so pre-1970 dates (negative Unix timestamp) work on
    // 64-bit Linux/MSVC builds.
    std::tm DateTime::toTm() const {
        const longcs unixTicks = ticks_ - UnixEpochTicks;
        // Floor division toward -inf (C++ truncates toward 0, which is wrong
        // for negative values — e.g. pre-1970 dates lose 1 second).
        longcs q = unixTicks / TicksPerSecond;
        const longcs r = unixTicks % TicksPerSecond;
        if (r < 0) --q;
        const time_t unixSec = static_cast<time_t>(q);
        std::tm result{};
#ifdef _WIN32
        gmtime_s(&result, &unixSec);
#else
        gmtime_r(&unixSec, &result);
#endif
        return result;
    }

    // Direct Gregorian → ticks formula (mirrors .NET internals).
    // Days are counted from 0001-01-01 (day 0 in .NET).
    longcs DateTime::dateToTicks(int year, int month, int day,
                                  int hour, int minute, int second, int millisecond)
    {
        if (year < 1 || year > 9999 || month < 1 || month > 12 || day < 1)
            throw std::out_of_range("DateTime: date component out of range");

        // Days-to-month tables for non-leap and leap years
        static const int d365[] = {0,31,59,90,120,151,181,212,243,273,304,334,365};
        static const int d366[] = {0,31,60,91,121,152,182,213,244,274,305,335,366};

        const bool leap = (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
        const int* d = leap ? d366 : d365;

        if (day > d[month] - d[month - 1])
            throw std::out_of_range("DateTime: day out of range for given month");

        const int y = year - 1;
        const longcs days = static_cast<longcs>(y) * 365
                          + y / 4 - y / 100 + y / 400
                          + d[month - 1] + day - 1;

        return days      * TicksPerDay
             + hour      * TicksPerHour
             + minute    * TicksPerMinute
             + second    * TicksPerSecond
             + millisecond * TicksPerMillisecond;
    }

    // -------------------------------------------------------------------------
    // Constructors
    // -------------------------------------------------------------------------

    DateTime::DateTime()
        : ticks_(0) {}

    DateTime::DateTime(longcs ticks)
        : ticks_(ticks) {}

    DateTime::DateTime(int year, int month, int day)
        : ticks_(dateToTicks(year, month, day)) {}

    DateTime::DateTime(int year, int month, int day, int hour, int minute, int second)
        : ticks_(dateToTicks(year, month, day, hour, minute, second)) {}

    DateTime::DateTime(int year, int month, int day,
                       int hour, int minute, int second, int millisecond)
        : ticks_(dateToTicks(year, month, day, hour, minute, second, millisecond)) {}

    // -------------------------------------------------------------------------
    // Properties — tick-level decomposition
    // -------------------------------------------------------------------------

    longcs DateTime::getTicksProperty() const { return ticks_; }

    int DateTime::getYearProperty()        const { return toTm().tm_year + 1900; }
    int DateTime::getMonthProperty()       const { return toTm().tm_mon  + 1;    }
    int DateTime::getDayProperty()         const { return toTm().tm_mday;         }
    int DateTime::getHourProperty()        const { return toTm().tm_hour;         }
    int DateTime::getMinuteProperty()      const { return toTm().tm_min;          }
    int DateTime::getSecondProperty()      const { return toTm().tm_sec;          }
    int DateTime::getMillisecondProperty() const {
        return static_cast<int>((ticks_ % TicksPerSecond) / TicksPerMillisecond);
    }

    DayOfWeek DateTime::getDayOfWeekProperty() const {
        return static_cast<DayOfWeek>(toTm().tm_wday); // 0=Sunday … 6=Saturday
    }

    int DateTime::getDayOfYearProperty() const {
        return toTm().tm_yday + 1; // tm_yday is 0-based, .NET is 1-based
    }

    // -------------------------------------------------------------------------
    // Arithmetic
    // -------------------------------------------------------------------------

    DateTime DateTime::Add(const TimeSpan& value) const {
        return DateTime(ticks_ + value.getTicksProperty());
    }

    DateTime DateTime::AddDays(int days) const {
        return DateTime(ticks_ + static_cast<longcs>(days) * TicksPerDay);
    }

    DateTime DateTime::AddHours(int hours) const {
        return DateTime(ticks_ + static_cast<longcs>(hours) * TicksPerHour);
    }

    DateTime DateTime::AddMinutes(int minutes) const {
        return DateTime(ticks_ + static_cast<longcs>(minutes) * TicksPerMinute);
    }

    DateTime DateTime::AddSeconds(int seconds) const {
        return DateTime(ticks_ + static_cast<longcs>(seconds) * TicksPerSecond);
    }

    DateTime DateTime::AddMilliseconds(int milliseconds) const {
        return DateTime(ticks_ + static_cast<longcs>(milliseconds) * TicksPerMillisecond);
    }

    DateTime DateTime::Subtract(const TimeSpan& value) const {
        return DateTime(ticks_ - value.getTicksProperty());
    }

    TimeSpan DateTime::Subtract(const DateTime& value) const {
        return TimeSpan(ticks_ - value.ticks_);
    }

    // -------------------------------------------------------------------------
    // Static factories
    // -------------------------------------------------------------------------

    DateTime DateTime::getNowProperty() {
        using namespace std::chrono;
        const auto now      = system_clock::now();
        const auto duration = now.time_since_epoch();
        const auto secs     = duration_cast<seconds>(duration);
        const auto sub      = duration - secs;
        const longcs ticks  = UnixEpochTicks
            + static_cast<longcs>(secs.count()) * TicksPerSecond
            + static_cast<longcs>(duration_cast<nanoseconds>(sub).count() / 100);
        return DateTime(ticks);
    }

    DateTime DateTime::getTodayProperty() {
        const DateTime now = getNowProperty();
        // Truncate to midnight
        return DateTime(now.ticks_ - (now.ticks_ % TicksPerDay));
    }

    TimeSpan DateTime::getTimeOfDayProperty() const {
        return TimeSpan(ticks_ % TicksPerDay);
    }

    // -------------------------------------------------------------------------
    // String
    // -------------------------------------------------------------------------

    std::string DateTime::ToString() const {
        const std::tm t = toTm();
        char buf[32];
        std::snprintf(buf, sizeof(buf),
            "%04d-%02d-%02d %02d:%02d:%02d",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec);
        return buf;
    }

    // -------------------------------------------------------------------------
    // Comparison operators
    // -------------------------------------------------------------------------

    bool DateTime::operator==(const DateTime& o) const { return ticks_ == o.ticks_; }
    bool DateTime::operator!=(const DateTime& o) const { return ticks_ != o.ticks_; }
    bool DateTime::operator< (const DateTime& o) const { return ticks_ <  o.ticks_; }
    bool DateTime::operator<=(const DateTime& o) const { return ticks_ <= o.ticks_; }
    bool DateTime::operator> (const DateTime& o) const { return ticks_ >  o.ticks_; }
    bool DateTime::operator>=(const DateTime& o) const { return ticks_ >= o.ticks_; }

    GetTypeNameCPP(DateTime, "System::DateTime")

} // namespace System
