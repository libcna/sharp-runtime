// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include "System/DateTimeOffset.hpp"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

namespace System {

    DateTimeOffset::DateTimeOffset()
        : dateTime_(DateTime()), offset_(TimeSpan::Zero) {}

    DateTimeOffset::DateTimeOffset(const DateTime& dateTime, const TimeSpan& offset)
        : dateTime_(dateTime), offset_(offset) {}

    // -------------------------------------------------------------------------
    // Static factory
    // -------------------------------------------------------------------------

    DateTimeOffset DateTimeOffset::getUtcNowProperty() {
        return DateTimeOffset(DateTime::getNowProperty(), TimeSpan::Zero);
    }

    DateTimeOffset DateTimeOffset::getNowProperty() {
        // system_clock::now() is UTC; get the local offset to compute local DateTime
        DateTime utc = DateTime::getNowProperty();
        long offset_secs = 0;
#if defined(_WIN32)
        TIME_ZONE_INFORMATION tz{};
        DWORD r = GetTimeZoneInformation(&tz);
        if (r != TIME_ZONE_ID_INVALID)
            offset_secs = -static_cast<long>(tz.Bias) * 60;
#elif defined(__EMSCRIPTEN__)
        offset_secs = 0;
#else
        std::time_t t = std::time(nullptr);
        struct tm local_tm{};
        localtime_r(&t, &local_tm);
        offset_secs = local_tm.tm_gmtoff;
#endif
        TimeSpan off = TimeSpan::FromSeconds(static_cast<double>(offset_secs));
        return DateTimeOffset(utc.Add(off), off);
    }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    const DateTime& DateTimeOffset::getDateTimeProperty() const { return dateTime_; }
    const TimeSpan& DateTimeOffset::getOffsetProperty()   const { return offset_; }

    longcs DateTimeOffset::getUtcTicksProperty() const {
        return dateTime_.getTicksProperty() - offset_.getTicksProperty();
    }

    intcs DateTimeOffset::getYearProperty()        const { return dateTime_.getYearProperty(); }
    intcs DateTimeOffset::getMonthProperty()       const { return dateTime_.getMonthProperty(); }
    intcs DateTimeOffset::getDayProperty()         const { return dateTime_.getDayProperty(); }
    intcs DateTimeOffset::getHourProperty()        const { return dateTime_.getHourProperty(); }
    intcs DateTimeOffset::getMinuteProperty()      const { return dateTime_.getMinuteProperty(); }
    intcs DateTimeOffset::getSecondProperty()      const { return dateTime_.getSecondProperty(); }
    intcs DateTimeOffset::getMillisecondProperty() const { return dateTime_.getMillisecondProperty(); }

    DateTime DateTimeOffset::getDateProperty() const {
        return DateTime(dateTime_.getYearProperty(), dateTime_.getMonthProperty(), dateTime_.getDayProperty());
    }

    DateTime DateTimeOffset::getUtcDateTimeProperty() const {
        return DateTime(getUtcTicksProperty());
    }

    // -------------------------------------------------------------------------
    // Arithmetic
    // -------------------------------------------------------------------------

    DateTimeOffset DateTimeOffset::Add(const TimeSpan& ts) const {
        return DateTimeOffset(dateTime_.Add(ts), offset_);
    }

    DateTimeOffset DateTimeOffset::AddDays(double days) const {
        return Add(TimeSpan::FromSeconds(days * 86400.0));
    }

    DateTimeOffset DateTimeOffset::AddHours(double hours) const {
        return Add(TimeSpan::FromHours(hours));
    }

    DateTimeOffset DateTimeOffset::AddMinutes(double minutes) const {
        return Add(TimeSpan::FromMinutes(minutes));
    }

    DateTimeOffset DateTimeOffset::AddSeconds(double seconds) const {
        return Add(TimeSpan::FromSeconds(seconds));
    }

    DateTimeOffset DateTimeOffset::AddMilliseconds(double ms) const {
        return Add(TimeSpan::FromSeconds(ms / 1000.0));
    }

    DateTimeOffset DateTimeOffset::AddMonths(intcs months) const {
        int y = getYearProperty(), m = getMonthProperty() + months, d = getDayProperty();
        y += (m - 1) / 12; m = ((m - 1) % 12 + 12) % 12 + 1;
        // clamp day to max days in month
        static const int maxDay[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
        int mx = maxDay[m];
        if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) mx = 29;
        if (d > mx) d = mx;
        DateTime newDt(y, m, d, getHourProperty(), getMinuteProperty(), getSecondProperty(), getMillisecondProperty());
        return DateTimeOffset(newDt, offset_);
    }

    DateTimeOffset DateTimeOffset::AddYears(intcs years) const {
        return AddMonths(years * 12);
    }

    TimeSpan DateTimeOffset::Subtract(const DateTimeOffset& other) const {
        return TimeSpan(getUtcTicksProperty() - other.getUtcTicksProperty());
    }

    DateTimeOffset DateTimeOffset::Subtract(const TimeSpan& ts) const {
        return DateTimeOffset(dateTime_.Subtract(ts), offset_);
    }

    DateTimeOffset DateTimeOffset::ToUniversalTime() const {
        return DateTimeOffset(getUtcDateTimeProperty(), TimeSpan::Zero);
    }

    // -------------------------------------------------------------------------
    // Parsing
    // -------------------------------------------------------------------------

    bool DateTimeOffset::TryParse(const std::string& s, DateTimeOffset& result) {
        if (s.size() < 10) return false;

        // Find where the offset starts (after the time part)
        // Look for 'Z' or '+'/'-' that appears after position 10
        std::string dtStr = s;
        TimeSpan offset = TimeSpan::Zero;

        // Check trailing 'Z'
        if (!s.empty() && (s.back() == 'Z' || s.back() == 'z')) {
            dtStr = s.substr(0, s.size() - 1);
            offset = TimeSpan::Zero;
        } else {
            // Look for '+' or '-' after position 10 (skip date separators)
            size_t offPos = std::string::npos;
            for (size_t i = 10; i < s.size(); ++i) {
                if (s[i] == '+' || s[i] == '-') { offPos = i; break; }
            }
            if (offPos != std::string::npos) {
                dtStr = s.substr(0, offPos);
                const std::string offStr = s.substr(offPos);
                bool neg = offStr[0] == '-';
                int hh = 0, mm = 0;
                if (std::sscanf(offStr.c_str() + 1, "%d:%d", &hh, &mm) != 2) return false;
                double secs = (hh * 3600.0 + mm * 60.0) * (neg ? -1.0 : 1.0);
                offset = TimeSpan::FromSeconds(secs);
            }
        }

        DateTime dt;
        if (!DateTime::TryParse(dtStr, dt)) return false;
        result = DateTimeOffset(dt, offset);
        return true;
    }

    DateTimeOffset DateTimeOffset::Parse(const std::string& s) {
        DateTimeOffset result;
        if (!TryParse(s, result))
            throw std::invalid_argument("String was not recognized as a valid DateTimeOffset.");
        return result;
    }

    // -------------------------------------------------------------------------
    // ToString
    // -------------------------------------------------------------------------

    static std::string formatOffset(const TimeSpan& off) {
        int totalMin = static_cast<int>(off.getTotalMinutesProperty());
        char sign = (totalMin < 0) ? '-' : '+';
        if (totalMin < 0) totalMin = -totalMin;
        int hh = totalMin / 60, mm = totalMin % 60;
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%c%02d:%02d", sign, hh, mm);
        return buf;
    }

    std::string DateTimeOffset::ToString() const {
        return dateTime_.ToString() + formatOffset(offset_);
    }

    std::string DateTimeOffset::ToString(const std::string& format) const {
        if (format == "O" || format == "o") {
            // ISO 8601 round-trip: yyyy-MM-ddTHH:mm:ss.fffffffzzz
            return dateTime_.ToString("yyyy-MM-ddTHH:mm:ss") + formatOffset(offset_);
        }
        if (format == "R" || format == "r") {
            // RFC 1123 (simplified — no day-of-week)
            return dateTime_.ToString("ddd, dd MMM yyyy HH:mm:ss") + " " +
                   (offset_ == TimeSpan::Zero ? "GMT" : formatOffset(offset_));
        }
        if (format == "u") {
            return dateTime_.ToString("yyyy-MM-dd HH:mm:ssZ");
        }
        // General format: delegate to DateTime and append offset
        return dateTime_.ToString(format) + formatOffset(offset_);
    }

    // -------------------------------------------------------------------------
    // Comparison
    // -------------------------------------------------------------------------

    intcs DateTimeOffset::CompareTo(const DateTimeOffset& other) const {
        longcs a = getUtcTicksProperty(), b = other.getUtcTicksProperty();
        return (a < b) ? -1 : (a > b) ? 1 : 0;
    }

    bool DateTimeOffset::Equals(const DateTimeOffset& other) const {
        return getUtcTicksProperty() == other.getUtcTicksProperty();
    }

    bool DateTimeOffset::operator==(const DateTimeOffset& other) const { return Equals(other); }
    bool DateTimeOffset::operator!=(const DateTimeOffset& other) const { return !Equals(other); }
    bool DateTimeOffset::operator< (const DateTimeOffset& other) const { return getUtcTicksProperty() <  other.getUtcTicksProperty(); }
    bool DateTimeOffset::operator<=(const DateTimeOffset& other) const { return getUtcTicksProperty() <= other.getUtcTicksProperty(); }
    bool DateTimeOffset::operator> (const DateTimeOffset& other) const { return getUtcTicksProperty() >  other.getUtcTicksProperty(); }
    bool DateTimeOffset::operator>=(const DateTimeOffset& other) const { return getUtcTicksProperty() >= other.getUtcTicksProperty(); }

    // -------------------------------------------------------------------------
    // Operators
    // -------------------------------------------------------------------------

    DateTimeOffset DateTimeOffset::operator+(const TimeSpan& ts) const { return Add(ts); }
    DateTimeOffset DateTimeOffset::operator-(const TimeSpan& ts) const { return Subtract(ts); }
    TimeSpan DateTimeOffset::operator-(const DateTimeOffset& other) const { return Subtract(other); }

    GetTypeNameCPP(DateTimeOffset, "System::DateTimeOffset")

} // namespace System
