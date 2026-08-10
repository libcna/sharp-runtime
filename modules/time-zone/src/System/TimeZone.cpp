// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/TimeZone.hpp"
#include "System/TimeZoneInfo.hpp"
#include "TimeZonePosixSupport.hpp"

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
 * The legacy TimeZone contract *is* per-date -- both members take the DateTime whose offset
 * is being asked about -- and this adapter only ever describes the process-local zone, which
 * POSIX can resolve per instant without the adapter storing anything. So it now answers per
 * date. TimeZoneInfo keeps its documented fixed-offset model; the asymmetry is deliberate and
 * is recorded in docs/SystemTimeZoneNamespaceReviewPlan.md section 12.
 *
 * The cached offset survives as the fallback for a date the platform cannot resolve, and as
 * the whole answer on Windows and Emscripten, where the previous behaviour is unchanged.
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
    static bool resolve(const DateTime& time, detail::ZoneSample& sample) {
        std::lock_guard<std::mutex> lock(detail::tzMutex());
        return detail::resolveLocalWallClock(
            static_cast<int>(time.getYearProperty()), static_cast<int>(time.getMonthProperty()),
            static_cast<int>(time.getDayProperty()), static_cast<int>(time.getHourProperty()),
            static_cast<int>(time.getMinuteProperty()),
            static_cast<int>(time.getSecondProperty()), sample);
    }

    TimeSpan GetUtcOffset(const DateTime& time) const override {
        detail::ZoneSample sample;
        if (!resolve(time, sample)) return offset_;
        return TimeSpan::FromSeconds(static_cast<double>(sample.utcOffsetSeconds));
    }

    bool IsDaylightSavingTime(const DateTime& time) const override {
        detail::ZoneSample sample;
        if (!resolve(time, sample)) return false;
        return sample.isDaylight;
    }
#else
    // Windows and Emscripten keep the frozen offset: neither has the POSIX helper, and the
    // Windows branch of TimeZoneInfo::Local() already records the standard offset rather than
    // a snapshot of the current one.
    TimeSpan GetUtcOffset(const DateTime& /*time*/) const override { return offset_; }
    bool IsDaylightSavingTime(const DateTime& /*time*/) const override { return false; }
#endif
};

} // anonymous namespace

const TimeZone& TimeZone::CurrentTimeZone() {
    static SystemTimeZoneAdapter instance;
    return instance;
}

} // namespace System
