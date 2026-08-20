// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include "System/TimeOnly.hpp"
#include "System/detail/DateTimeTextScanner.hpp"
#include "System/DateTime.hpp"
#include "System/FormatException.hpp"
#include "System/detail/InvariantExactDateTimeParser.hpp"
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
    long long ticks = ts.getTicksProperty() % TicksPerDay_;
    if (ticks < 0) ticks += TicksPerDay_;
    return TimeOnly(ticks);
}

TimeOnly TimeOnly::FromDateTime(const DateTime& dt) {
    return TimeOnly(dt.getTicksProperty() % TicksPerDay_);
}

bool TimeOnly::TryParse(const std::string& s, TimeOnly& result) {
    // Ticket #1880: every false result assigns TimeOnly.MinValue, matching
    // .NET's out contract while leaving the success path commit-only.
    const auto fail = [&result]() {
        result = TimeOnly::getMinValueProperty();
        return false;
    };
    // CCF-002 class D (SR-AUD-009, ticket #1879, approved 2026-07-31).
    //
    // This used to run one std::sscanf PREFIX conversion and then hand-walk the
    // fraction, never asking whether the whole string had been consumed -- so
    // "10:20:30junk" was a valid time, "10:20:30." and "10:20:30.abc" were read
    // as .000, and "10:20:30.1234" was truncated to .123. The grammar below is
    // required to match the WHOLE string:
    //
    //     W* H{1,2} ':' m{1,2} ':' s{1,2} [ '.' f{1,7} ] W*
    //
    // One-or-two-digit fields are DELIBERATE and unchanged: "1:2:3" parses today
    // and .NET's TimeOnly.Parse accepts it too (its "H:m:s" standard pattern), so
    // requiring zero padding here would be a narrowing this approval does not
    // cover and a divergence from .NET rather than a step towards it. The
    // *offset* fields in DateTimeOffset are the ones §20.1 requires to be
    // two-digit, and they are handled there.
    // Correction/remediation (#1929 rows 5-6, approved 2026-08-01): TimeOnly's
    // tick constructor and related types are 100ns-based, so the historical
    // claim that this representation could retain only milliseconds was false.
    // The existing fourth int now retains ticks within the second without any
    // size/alignment/layout change.
    detail::DateTimeTextScanner scanner(detail::trimDateTimeText(s));
    int h = 0, m = 0, sc = 0, fractionTicks = 0;
    if (!scanner.takeDigits(1, 2, h) || !scanner.take(':') ||
        !scanner.takeDigits(1, 2, m) || !scanner.take(':') ||
        !scanner.takeDigits(1, 2, sc))
        return fail();
    if (scanner.take('.')) {
        int digits = 0;
        if (!scanner.takeDigits(1, 7, fractionTicks, &digits)) return fail();
        while (digits < 7) { fractionTicks *= 10; ++digits; }
    }
    if (!scanner.atEnd()) return fail();
    if (h < 0 || h > 23 || m < 0 || m > 59 || sc < 0 || sc > 59) return fail();

    TimeOnly parsed;
    parsed.hour_ = h;
    parsed.minute_ = m;
    parsed.second_ = sc;
    parsed.subsecondTicks_ = fractionTicks;
    result = parsed;
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
            std::string fraction = pad(subsecondTicks_, 7);
            // Approval covers f through fffffff. Preserve the old three-digit
            // fallback for a longer unsupported run rather than widening it too.
            const int width = (n <= 7) ? n : 3;
            result += fraction.substr(0, static_cast<size_t>(width));
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

// ---------------------------------------------------------------------------
// ParseExact / TryParseExact -- ticket #1939 (#1929 row 4A)
// ---------------------------------------------------------------------------

namespace System {

    bool TimeOnly::TryParseExact(const std::string& input, const std::string& format,
                                 TimeOnly& result) {
        // #2412: the two-argument form is the null-provider, no-style case of the four-argument
        // one. Delegating rather than duplicating is what makes "no existing result changes" a
        // fact -- there is no second scanner path left to drift.
        return TryParseExact(input, format, nullptr,
                             System::Globalization::DateTimeStyles::None, result);
    }

    bool TimeOnly::TryParseExact(const std::string& input, const std::string& format,
                                 const System::IFormatProvider* provider,
                                 System::Globalization::DateTimeStyles style, TimeOnly& result) {
        // .NET raises here rather than returning false -- a Try* method that throws
        // (TimeOnly.cs). Validation happens BEFORE the result is written, so a rejected style
        // leaves the caller's variable untouched.
        detail::ValidateDateTimeOnlyStyles(style);
        const detail::ExactParseOptions options = detail::ResolveExactParseOptions(provider, style);

        result = TimeOnly::getMinValueProperty();

        std::string pattern = detail::ExpandStandardFormat(format, /*forDate=*/false);
        if (pattern.empty()) {
            if (format.size() == 1) return false;   // see DateOnly::TryParseExact for why
            pattern = format;
        }

        detail::ExactDateTimeFields fields;
        if (!detail::MatchExactFormat(input, pattern, /*forDate=*/false, fields, options)) return false;

        // One hour and one minute are required; seconds are optional and default to zero.
        if (fields.hour < 0 || fields.minute < 0) return false;
        const int second = fields.second < 0 ? 0 : fields.second;

        // A 12-HOUR FORM REQUIRES `t`, and a 24-hour form must not carry one. Without this,
        // "01:30" under "hh:mm" would silently mean 01:30 with no way to say half past one in the
        // afternoon -- the designator is what carries that half of the value.
        if (fields.twelveHour && fields.amPm < 0) return false;
        if (!fields.twelveHour && fields.amPm >= 0) return false;

        int hour = fields.hour;
        if (fields.twelveHour) {
            if (hour < 1 || hour > 12) return false;
            if (fields.amPm == 1) hour = (hour == 12) ? 12 : hour + 12;
            else                  hour = (hour == 12) ? 0  : hour;
        } else if (hour > 23) {
            return false;
        }
        if (fields.minute > 59 || second > 59) return false;

        try {
            result = TimeOnly(static_cast<SharpRuntime::longcs>(
                hour * TimeSpan::TicksPerHour + fields.minute * TimeSpan::TicksPerMinute +
                second * TimeSpan::TicksPerSecond + fields.fractionTicks));
        } catch (...) {
            result = TimeOnly::getMinValueProperty();
            return false;
        }
        return true;
    }

    TimeOnly TimeOnly::ParseExact(const std::string& input, const std::string& format) {
        TimeOnly result;
        if (!TryParseExact(input, format, result))
            throw FormatException("String was not recognized as a valid TimeOnly: " + input);
        return result;
    }

    TimeOnly TimeOnly::ParseExact(const std::string& input, const std::string& format,
                                 const System::IFormatProvider* provider,
                                 System::Globalization::DateTimeStyles style) {
        TimeOnly result = TimeOnly::getMinValueProperty();
        // The style is validated by TryParseExact below, which raises for an illegal one exactly
        // as .NET's does; a PARSE failure is what becomes the FormatException here.
        if (!TryParseExact(input, format, provider, style, result))
            throw FormatException("String was not recognized as a valid TimeOnly: " + input);
        return result;
    }

} // namespace System
