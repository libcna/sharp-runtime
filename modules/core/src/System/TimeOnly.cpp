// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include "System/TimeOnly.hpp"
#include "System/detail/DateTimeTextScanner.hpp"
#include "System/DateTime.hpp"
#include "System/FormatException.hpp"
#include <algorithm>
#include <cstdio>

namespace System {

static constexpr long long TicksPerHour_ = 36000000000LL;
static constexpr long long TicksPerMinute_ = 600000000LL;
static constexpr long long TicksPerDay_ = 864000000000LL;

static TimeOnly AddTicksWrapped(long long baseTicks, long long deltaTicks) {
    long long t = (baseTicks + TicksPerDay_ + (deltaTicks % TicksPerDay_)) % TicksPerDay_;
    return TimeOnly(static_cast<SharpRuntime::longcs>(t));
}

TimeOnly TimeOnly::AddHours(double value) const {
    return AddTicksWrapped(getTicksProperty(), static_cast<long long>(value * static_cast<double>(TicksPerHour_)));
}

TimeOnly TimeOnly::AddMinutes(double value) const {
    return AddTicksWrapped(getTicksProperty(), static_cast<long long>(value * static_cast<double>(TicksPerMinute_)));
}

TimeOnly TimeOnly::FromTimeSpan(const TimeSpan& ts) {
    long long totalMs = static_cast<long long>(ts.getTotalMillisecondsProperty());
    int ms = static_cast<int>(totalMs % MsPerDay);
    if (ms < 0) ms += MsPerDay;
    return TimeOnly(ms / 3600000, (ms % 3600000) / 60000,
                    (ms % 60000) / 1000, ms % 1000);
}

TimeOnly TimeOnly::FromDateTime(const DateTime& dt) {
    return TimeOnly(dt.getHourProperty(), dt.getMinuteProperty(),
                    dt.getSecondProperty(), dt.getMillisecondProperty());
}

bool TimeOnly::TryParse(const std::string& s, TimeOnly& result) {
    // CCF-002 class D (SR-AUD-009, ticket #1879, approved 2026-07-31).
    //
    // This used to run one std::sscanf PREFIX conversion and then hand-walk the
    // fraction, never asking whether the whole string had been consumed -- so
    // "10:20:30junk" was a valid time, "10:20:30." and "10:20:30.abc" were read
    // as .000, and "10:20:30.1234" was truncated to .123. The grammar below is
    // required to match the WHOLE string:
    //
    //     H{1,2} ':' m{1,2} ':' s{1,2} [ '.' f{1,3} ]
    //
    // One-or-two-digit fields are DELIBERATE and unchanged: "1:2:3" parses today
    // and .NET's TimeOnly.Parse accepts it too (its "H:m:s" standard pattern), so
    // requiring zero padding here would be a narrowing this approval does not
    // cover and a divergence from .NET rather than a step towards it. The
    // *offset* fields in DateTimeOffset are the ones §20.1 requires to be
    // two-digit, and they are handled there.
    detail::DateTimeTextScanner scanner(s);
    int h = 0, m = 0, sc = 0, ms = 0;
    if (!scanner.takeDigits(1, 2, h) || !scanner.take(':') ||
        !scanner.takeDigits(1, 2, m) || !scanner.take(':') ||
        !scanner.takeDigits(1, 2, sc))
        return false;
    if (scanner.take('.')) {
        int digits = 0;
        if (!scanner.takeDigits(1, 3, ms, &digits)) return false;
        while (digits < 3) { ms *= 10; ++digits; }
    }
    if (!scanner.atEnd()) return false;
    if (h < 0 || h > 23 || m < 0 || m > 59 || sc < 0 || sc > 59) return false;

    result = TimeOnly(h, m, sc, ms);
    return true;
}

TimeOnly TimeOnly::Parse(const std::string& s) {
    TimeOnly result;
    if (!TryParse(s, result))
        throw FormatException("String was not recognized as a valid TimeOnly: " + s);
    return result;
}

std::string TimeOnly::ToString(const std::string& format) const {
    std::string result;
    result.reserve(format.size() + 4);
    auto pad = [](int n, int w) -> std::string {
        std::string s = std::to_string(n);
        while (static_cast<int>(s.size()) < w) s = "0" + s;
        return s;
    };
    size_t i = 0;
    while (i < format.size()) {
        char c = format[i];
        auto run = [&](char ch) {
            size_t k = i + 1;
            while (k < format.size() && format[k] == ch) ++k;
            return static_cast<int>(k - i);
        };
        if (c == 'H') {
            int n = run('H');
            result += (n >= 2) ? pad(hour_, 2) : std::to_string(hour_);
            i += n;
        } else if (c == 'h') {
            int n = run('h');
            int h12 = hour_ % 12; if (h12 == 0) h12 = 12;
            result += (n >= 2) ? pad(h12, 2) : std::to_string(h12);
            i += n;
        } else if (c == 'm') {
            int n = run('m');
            result += (n >= 2) ? pad(minute_, 2) : std::to_string(minute_);
            i += n;
        } else if (c == 's') {
            int n = run('s');
            result += (n >= 2) ? pad(second_, 2) : std::to_string(second_);
            i += n;
        } else if (c == 'f') {
            int n = run('f');
            std::string mss = pad(ms_, 3);
            result += mss.substr(0, static_cast<size_t>(std::min(n, 3)));
            i += n;
        } else if (c == '\'') {
            ++i;
            while (i < format.size() && format[i] != '\'') result += format[i++];
            if (i < format.size()) ++i;
        } else {
            result += c;
            ++i;
        }
    }
    return result;
}

} // namespace System
