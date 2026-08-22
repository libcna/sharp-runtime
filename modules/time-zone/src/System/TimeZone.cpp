// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/TimeZone.hpp"
#include "System/TimeZoneInfo.hpp"
#include "TimeZonePosixSupport.hpp"

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace System {

namespace {

/**
 * Concrete adapter that wraps the system time zone as a legacy TimeZone.
 * Used only by CurrentTimeZone().
 *
 * Ticket #2182 (SR-AUD-223). This adapter used to snapshot
 * TimeZoneInfo::Local().BaseUtcOffset once, at static construction, and return that one
 * value from GetUtcOffset() for every date; IsDaylightSavingTime() was hard-coded to false.
 * Under TZ=America/New_York it therefore answered -240 minutes and "no daylight time" for
 * both January and July, where the system database says -18000 s / isdst 0 and -14400 s /
 * isdst 1. Those were two independent defects in one file, and fixing either alone still
 * leaves the other wrong.
 *
 * The legacy TimeZone contract *is* per-date, but it asks two different questions. Public
 * GetUtcOffset/IsDaylightSavingTime interpret Local and Unspecified fields as a local wall clock
 * and give Utc values zero/false. DateTime::ToLocalTime instead needs the offset selected by a
 * UTC instant. Treating those UTC fields as a local clock picks the wrong side of DST boundaries,
 * so ILocalTimeZone exposes a separate internal conversion query and this adapter implements both.
 *
 * The cached offset survives only as the fallback for a date the platform cannot resolve. POSIX
 * and Windows both use date-sensitive system rules; Emscripten has no zone database and keeps
 * its documented zero-offset Local model.
 */
class SystemTimeZoneAdapter final : public TimeZone {
    std::string standardName_;
    std::string daylightName_;
    TimeSpan    offset_;

public:
    SystemTimeZoneAdapter()
        : standardName_(TimeZoneInfo::Local().getStandardNameProperty()),
          daylightName_(TimeZoneInfo::Local().getDaylightNameProperty()),
          offset_(TimeZoneInfo::Local().getBaseUtcOffsetProperty()) {}

    const std::string& getStandardNameProperty() const override { return standardName_; }
    const std::string& getDaylightNameProperty() const override { return daylightName_; }

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    /**
     * Resolves @p time as a local wall-clock time in the process zone. Returns false when the
     * platform cannot resolve it at all, in which case the caller keeps the standard offset.
     */
    static bool resolveLocal(const DateTime& time, detail::ZoneSample& sample) {
        std::lock_guard<std::mutex> lock(detail::processTimeZoneMutex());
        return detail::resolveLocalWallClock(
            static_cast<int>(time.getYearProperty()), static_cast<int>(time.getMonthProperty()),
            static_cast<int>(time.getDayProperty()), static_cast<int>(time.getHourProperty()),
            static_cast<int>(time.getMinuteProperty()),
            static_cast<int>(time.getSecondProperty()), sample);
    }

    /** Resolves @p time's fields as a UTC instant, for DateTime::ToLocalTime. */
    static bool resolveUniversal(const DateTime& time, detail::ZoneSample& sample) {
        std::lock_guard<std::mutex> lock(detail::processTimeZoneMutex());
        return detail::resolveUtcInstant(
            static_cast<int>(time.getYearProperty()), static_cast<int>(time.getMonthProperty()),
            static_cast<int>(time.getDayProperty()), static_cast<int>(time.getHourProperty()),
            static_cast<int>(time.getMinuteProperty()),
            static_cast<int>(time.getSecondProperty()), sample);
    }

    TimeSpan GetUtcOffset(const DateTime& time) const override {
        // CurrentSystemTimeZone.GetUtcOffset(DateTime) returns zero for Utc. The hidden
        // UTC-to-local conversion path is deliberately separate below.
        if (time.getKindProperty() == DateTimeKind::Utc) return TimeSpan::Zero;
        detail::ZoneSample sample;
        if (!resolveLocal(time, sample)) return offset_;
        return TimeSpan::FromSeconds(static_cast<double>(sample.utcOffsetSeconds));
    }

    TimeSpan GetUtcOffsetFromUniversalTime(const DateTime& time) const override {
        detail::ZoneSample sample;
        if (!resolveUniversal(time, sample)) return offset_;
        return TimeSpan::FromSeconds(static_cast<double>(sample.utcOffsetSeconds));
    }

    bool IsDaylightSavingTime(const DateTime& time) const override {
        if (time.getKindProperty() == DateTimeKind::Utc) return false;
        detail::ZoneSample sample;
        if (!resolveLocal(time, sample)) return false;
        return sample.isDaylight;
    }
#elif defined(_WIN32)
    static SYSTEMTIME toSystemTime(const DateTime& time) {
        SYSTEMTIME value{};
        value.wYear = static_cast<WORD>(time.getYearProperty());
        value.wMonth = static_cast<WORD>(time.getMonthProperty());
        value.wDay = static_cast<WORD>(time.getDayProperty());
        value.wHour = static_cast<WORD>(time.getHourProperty());
        value.wMinute = static_cast<WORD>(time.getMinuteProperty());
        value.wSecond = static_cast<WORD>(time.getSecondProperty());
        value.wMilliseconds = static_cast<WORD>(time.getMillisecondProperty());
        return value;
    }

    static bool zoneForYear(WORD year, TIME_ZONE_INFORMATION& result) {
        DYNAMIC_TIME_ZONE_INFORMATION dynamic{};
        if (GetDynamicTimeZoneInformation(&dynamic) == TIME_ZONE_ID_INVALID) return false;
        return GetTimeZoneInformationForYear(year, &dynamic, &result) != FALSE;
    }

    static bool clockDifference(const SYSTEMTIME& local, const SYSTEMTIME& utc,
                                TimeSpan& offset) {
        FILETIME localFile{}, utcFile{};
        if (SystemTimeToFileTime(&local, &localFile) == FALSE ||
            SystemTimeToFileTime(&utc, &utcFile) == FALSE)
            return false;
        ULARGE_INTEGER localTicks{}, utcTicks{};
        localTicks.LowPart = localFile.dwLowDateTime;
        localTicks.HighPart = localFile.dwHighDateTime;
        utcTicks.LowPart = utcFile.dwLowDateTime;
        utcTicks.HighPart = utcFile.dwHighDateTime;
        const unsigned long long magnitude = localTicks.QuadPart >= utcTicks.QuadPart
            ? localTicks.QuadPart - utcTicks.QuadPart
            : utcTicks.QuadPart - localTicks.QuadPart;
        const longcs signedTicks = localTicks.QuadPart >= utcTicks.QuadPart
            ? static_cast<longcs>(magnitude)
            : -static_cast<longcs>(magnitude);
        offset = TimeSpan(signedTicks);
        return true;
    }

    TimeSpan GetUtcOffset(const DateTime& time) const override {
        if (time.getKindProperty() == DateTimeKind::Utc) return TimeSpan::Zero;
        const SYSTEMTIME local = toSystemTime(time);
        TIME_ZONE_INFORMATION zone{};
        SYSTEMTIME utc{};
        TimeSpan resolved = offset_;
        if (!zoneForYear(local.wYear, zone) ||
            TzSpecificLocalTimeToSystemTime(&zone, &local, &utc) == FALSE ||
            !clockDifference(local, utc, resolved))
            return offset_;
        return resolved;
    }

    TimeSpan GetUtcOffsetFromUniversalTime(const DateTime& time) const override {
        const SYSTEMTIME utc = toSystemTime(time);
        TIME_ZONE_INFORMATION zone{};
        SYSTEMTIME local{};
        TimeSpan resolved = offset_;
        if (!zoneForYear(utc.wYear, zone) ||
            SystemTimeToTzSpecificLocalTime(&zone, &utc, &local) == FALSE ||
            !clockDifference(local, utc, resolved))
            return offset_;
        return resolved;
    }

    bool IsDaylightSavingTime(const DateTime& time) const override {
        if (time.getKindProperty() == DateTimeKind::Utc) return false;
        TIME_ZONE_INFORMATION zone{};
        if (!zoneForYear(static_cast<WORD>(time.getYearProperty()), zone)) return false;
        const TimeSpan standard = TimeSpan::FromMinutes(
            static_cast<double>(-(zone.Bias + zone.StandardBias)));
        return GetUtcOffset(time) != standard;
    }
#else
    // Emscripten has no process timezone database; its documented local-zone policy is UTC.
    TimeSpan GetUtcOffset(const DateTime& time) const override {
        return time.getKindProperty() == DateTimeKind::Utc ? TimeSpan::Zero : offset_;
    }
    TimeSpan GetUtcOffsetFromUniversalTime(const DateTime& /*time*/) const override {
        return offset_;
    }
    bool IsDaylightSavingTime(const DateTime& /*time*/) const override { return false; }
#endif
};

} // anonymous namespace

const TimeZone& TimeZone::CurrentTimeZone() {
    static SystemTimeZoneAdapter instance;
    return instance;
}

} // namespace System
