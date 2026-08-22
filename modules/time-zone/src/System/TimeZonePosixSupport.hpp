// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

// Private implementation header for the SharpRuntime::TimeZone component. It is deliberately
// NOT under include/: it exposes POSIX types and process-global timezone state, neither of
// which may appear in a public header. Only TimeZoneInfo.cpp and TimeZone.cpp include it.

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)

#include "System/detail/ProcessTimeZoneState.hpp"
#include <cstdlib>
#include <ctime>
#include <string>

namespace System::detail {

    /**
     * @brief Saves TZ on construction and restores it on destruction, on every path.
     *
     * The previous code saved TZ, did work that can throw (at minimum a std::string
     * assignment), and restored TZ with straight-line statements, so an exception left the
     * process-global zone pointing at whatever zone was being probed -- for the whole process,
     * for the rest of its life. This guard makes the restore unconditional.
     *
     * It also distinguishes "TZ was unset" from "TZ was set to the empty string". POSIX gives
     * those different meanings -- an empty TZ selects UTC, an absent TZ selects the system
     * default -- and the previous restore branched on `savedStr.empty()`, so an empty-but-set
     * TZ was unsetenv()d rather than restored.
     *
     * The caller must already hold processTimeZoneMutex(); the guard does not lock, because the
     * same lock has to cover the localtime_r() reads that follow the setenv().
     */
    class ScopedTz {
        std::string saved_;
        bool        wasSet_;

    public:
        ScopedTz() {
            const char* current = getenv("TZ");
            wasSet_ = current != nullptr;
            if (wasSet_) saved_ = current;   // copied immediately: getenv's storage is volatile
        }

        ScopedTz(const ScopedTz&)            = delete;
        ScopedTz& operator=(const ScopedTz&) = delete;
        ScopedTz(ScopedTz&&)                 = delete;
        ScopedTz& operator=(ScopedTz&&)      = delete;

        /** @brief Points the process at @p zoneId for the lifetime of this guard. */
        void select(const std::string& zoneId) const {
            setenv("TZ", zoneId.c_str(), 1);
            tzset();
        }

        ~ScopedTz() {
            if (wasSet_) setenv("TZ", saved_.c_str(), 1);
            else         unsetenv("TZ");
            tzset();
        }
    };

    /**
     * @brief The offset, DST flag and abbreviation the currently selected zone gives an instant.
     */
    struct ZoneSample {
        long        utcOffsetSeconds = 0;
        bool        isDaylight       = false;
        std::string abbreviation;
    };

    /**
     * @brief Samples the currently selected zone at the UTC instant @p year-@p month-15 12:00.
     *
     * Midday on the 15th is used rather than midnight on the 1st so that no sample can land on
     * a transition instant in any installed zone. Callers must already hold
     * processTimeZoneMutex() and have selected the zone they mean to sample.
     */
    inline ZoneSample sampleZoneAtMonth(int year, int month) {
        struct tm utc {};
        utc.tm_year = year - 1900;
        utc.tm_mon  = month;
        utc.tm_mday = 15;
        utc.tm_hour = 12;
        time_t instant = timegm(&utc);
        struct tm local {};
        localtime_r(&instant, &local);
        ZoneSample s;
        s.utcOffsetSeconds = local.tm_gmtoff;
        s.isDaylight       = local.tm_isdst > 0;
        s.abbreviation     = local.tm_zone ? local.tm_zone : "";
        return s;
    }

    /**
     * @brief The invariant metadata of a zone, derived from a whole year rather than one instant.
     */
    struct ZoneMetadata {
        long        standardOffsetSeconds = 0;
        std::string standardAbbreviation;
        std::string daylightAbbreviation;
        bool        observesDaylight = false;
    };

    /**
     * @brief Derives a zone's standard offset, both abbreviations, and whether it observes DST.
     *
     * Reading tm_gmtoff and tm_zone at time(nullptr) -- what this component used to do -- makes
     * BaseUtcOffset, StandardName and DaylightName depend on the month the process happens to
     * run in. Measured over the 499 TZif zones installed here, 158 observe DST and 141 reported
     * the wrong base offset on 2026-08-10 alone; the other 17 are southern-hemisphere zones,
     * which are wrong in January instead.
     *
     * Scanning twelve months of @p year fixes that. The standard values are taken from the
     * first sample whose tm_isdst is 0, and the fallback below covers a zone the database
     * marks as on daylight time for every month scanned.
     *
     * Two measurements bound that fallback honestly. First, it is **defensive and currently
     * unexercised**: not one of the 499 installed zones lacks a standard-time sample in 2025.
     * Africa/Casablanca comes closest and is why twelve samples are taken rather than the two
     * this replaces -- both January and July are daylight time there, so a Jan/Jul probe sees
     * no standard sample at all, while the twelve-month scan finds the Ramadan reversion and
     * reports +00 instead of +01. Second, where the fallback would apply, "minimum offset
     * across the year" agrees with "first tm_isdst == 0 sample" for all 499 installed zones,
     * so it cannot disagree with the primary rule on any zone that exists here.
     *
     * Callers must already hold processTimeZoneMutex() and have selected the zone they mean to
     * describe.
     */
    inline ZoneMetadata describeSelectedZone(int year) {
        ZoneMetadata meta;
        bool sawStandard = false;
        bool haveExtremes = false;
        long minimumOffset = 0;
        long maximumOffset = 0;
        std::string minimumAbbreviation;

        for (int month = 0; month < 12; ++month) {
            ZoneSample s = sampleZoneAtMonth(year, month);
            if (s.isDaylight) {
                meta.observesDaylight = true;
                if (meta.daylightAbbreviation.empty()) meta.daylightAbbreviation = s.abbreviation;
            } else if (!sawStandard) {
                sawStandard                = true;
                meta.standardOffsetSeconds = s.utcOffsetSeconds;
                meta.standardAbbreviation  = s.abbreviation;
            }
            if (!haveExtremes) {
                haveExtremes        = true;
                minimumOffset       = s.utcOffsetSeconds;
                maximumOffset       = s.utcOffsetSeconds;
                minimumAbbreviation = s.abbreviation;
            } else if (s.utcOffsetSeconds < minimumOffset) {
                minimumOffset       = s.utcOffsetSeconds;
                minimumAbbreviation = s.abbreviation;
            } else if (s.utcOffsetSeconds > maximumOffset) {
                maximumOffset = s.utcOffsetSeconds;
            }
        }

        // A zone whose offset varies across the year observes daylight time even where the
        // database never sets tm_isdst; the Jan/Jul comparison this replaces used the same
        // signal, and dropping it would silently narrow SupportsDaylightSavingTime.
        if (minimumOffset != maximumOffset) meta.observesDaylight = true;

        if (!sawStandard) {
            meta.standardOffsetSeconds = minimumOffset;
            meta.standardAbbreviation  = minimumAbbreviation;
        }
        if (meta.daylightAbbreviation.empty()) meta.daylightAbbreviation = meta.standardAbbreviation;
        return meta;
    }

    /**
     * @brief The UTC year of the current instant, used as the year the metadata scan describes.
     */
    inline int currentUtcYear() {
        time_t now = time(nullptr);
        struct tm utc {};
        gmtime_r(&now, &utc);
        return utc.tm_year + 1900;
    }

    /**
     * @brief The offset and DST flag the currently selected zone gives a local wall-clock time.
     *
     * Used by the legacy TimeZone adapter, whose contract is per-date. mktime() with
     * tm_isdst = -1 resolves the daylight question from the zone's own rules.
     *
     * That alone is not enough, and the reason is measured in
     * build-probe/2176_probe6_ambiguous.log: for a *repeated* local hour glibc's mktime
     * inherits the daylight state of whatever conversion happened before it in the same
     * process. 2025-11-02 01:30 in America/New_York answers -240 as the first call in a
     * process, -300 immediately after a standard-time conversion and -240 immediately after a
     * daylight one. The same argument giving different answers depending on unrelated earlier
     * calls is a defect on its own terms, whatever .NET does.
     *
     * So an hour that resolved to daylight time is re-asked as standard time, and the standard
     * answer is taken only when it reproduces the requested wall clock exactly -- which happens
     * precisely when the wall clock exists in both readings, i.e. when it is ambiguous. An
     * ordinary summer time (12:00 in July) does not reproduce, because asking for it as
     * standard time normalises the fields forward, so it keeps its daylight answer. A local
     * time inside a spring-forward gap does not reproduce either, and measurement shows it was
     * already stable: 02:30 on 2025-03-09 answers -240 in every ordering.
     *
     * Preferring the standard reading for an ambiguous hour is also what .NET documents for
     * TimeZoneInfo.GetUtcOffset. The determinism is a defect repair; the *choice* of which
     * reading to prefer is the part that rests on that recollection, and it is carried by
     * #2186 for verification against the reference.
     *
     * @param year..@p second The local wall-clock time to resolve.
     * @param out             Receives the offset, DST flag and abbreviation.
     * @return false if the platform could not resolve the time at all, leaving @p out untouched.
     */
    inline bool resolveLocalWallClock(int year, int month, int day, int hour, int minute,
                                      int second, ZoneSample& out) {
        auto fill = [&](struct tm& t, int isdst) {
            t          = tm{};
            t.tm_year  = year - 1900;
            t.tm_mon   = month - 1;
            t.tm_mday  = day;
            t.tm_hour  = hour;
            t.tm_min   = minute;
            t.tm_sec   = second;
            t.tm_isdst = isdst;
        };
        auto reproducesRequest = [&](const struct tm& t) {
            return t.tm_year == year - 1900 && t.tm_mon == month - 1 && t.tm_mday == day &&
                   t.tm_hour == hour && t.tm_min == minute && t.tm_sec == second;
        };

        struct tm local {};
        fill(local, -1);
        time_t instant = mktime(&local);
        if (instant == static_cast<time_t>(-1) && local.tm_isdst < 0) return false;

        if (local.tm_isdst > 0) {
            struct tm standard {};
            fill(standard, 0);
            time_t standardInstant = mktime(&standard);
            if (!(standardInstant == static_cast<time_t>(-1) && standard.tm_isdst < 0) &&
                standard.tm_isdst == 0 && reproducesRequest(standard)) {
                local = standard;
            }
        }

        out.utcOffsetSeconds = local.tm_gmtoff;
        out.isDaylight       = local.tm_isdst > 0;
        out.abbreviation     = local.tm_zone ? local.tm_zone : "";
        return true;
    }

    /**
     * @brief Resolves the selected zone's offset at an exact UTC instant.
     *
     * This is intentionally separate from resolveLocalWallClock(). Around a DST transition the
     * same calendar fields denote different questions: `2025-03-09 07:30` as a New York wall
     * clock is already daylight time, while `07:30Z` maps to `03:30` after the spring jump. A
     * UTC-to-local conversion must choose by the instant and must not feed UTC fields to
     * mktime(), which interprets them as local.
     *
     * The `time_t(-1)` value is not rejected on its own: it is also the valid instant
     * 1969-12-31T23:59:59Z. A gmtime round trip is the representability check. The caller must
     * already hold processTimeZoneMutex().
     */
    inline bool resolveUtcInstant(int year, int month, int day, int hour, int minute,
                                  int second, ZoneSample& out) {
        struct tm utc {};
        utc.tm_year = year - 1900;
        utc.tm_mon  = month - 1;
        utc.tm_mday = day;
        utc.tm_hour = hour;
        utc.tm_min  = minute;
        utc.tm_sec  = second;

        const time_t instant = timegm(&utc);
        struct tm roundTrip {};
        if (gmtime_r(&instant, &roundTrip) == nullptr ||
            roundTrip.tm_year != year - 1900 || roundTrip.tm_mon != month - 1 ||
            roundTrip.tm_mday != day || roundTrip.tm_hour != hour ||
            roundTrip.tm_min != minute || roundTrip.tm_sec != second) {
            return false;
        }

        struct tm local {};
        if (localtime_r(&instant, &local) == nullptr) return false;
        out.utcOffsetSeconds = local.tm_gmtoff;
        out.isDaylight       = local.tm_isdst > 0;
        out.abbreviation     = local.tm_zone ? local.tm_zone : "";
        return true;
    }

} // namespace System::detail

#endif // !_WIN32 && !__EMSCRIPTEN__
