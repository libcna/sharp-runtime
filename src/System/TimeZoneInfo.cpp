// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/TimeZoneInfo.hpp"
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <sys/stat.h>

namespace System {

// ---------------------------------------------------------------------------
// Local() — reads the real system timezone via POSIX localtime_r()
// ---------------------------------------------------------------------------

const TimeZoneInfo& TimeZoneInfo::Local() {
    static TimeZoneInfo tz = []() -> TimeZoneInfo {
        time_t t = time(nullptr);
        struct tm local_tm {};
        localtime_r(&t, &local_tm);
        long   offset_secs = local_tm.tm_gmtoff;
        bool   hasDst      = (local_tm.tm_isdst > 0);
        std::string name   = local_tm.tm_zone ? local_tm.tm_zone : "Local";
        TimeSpan offset    = TimeSpan::FromSeconds(static_cast<double>(offset_secs));
        return TimeZoneInfo("Local", offset, name, name, name, hasDst);
    }();
    return tz;
}

// ---------------------------------------------------------------------------
// FindSystemTimeZoneById() — resolves IANA IDs via /usr/share/zoneinfo/
// ---------------------------------------------------------------------------

static bool zoneFileExists(const std::string& id) {
    // Guard against path traversal
    if (id.find("..") != std::string::npos) return false;
    std::string path = "/usr/share/zoneinfo/" + id;
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

static std::mutex s_tzMutex;

std::shared_ptr<TimeZoneInfo> TimeZoneInfo::FindSystemTimeZoneById(const std::string& id) {
    if (id == "UTC")   return std::shared_ptr<TimeZoneInfo>(new TimeZoneInfo(Utc()));
    if (id == "Local") return std::shared_ptr<TimeZoneInfo>(new TimeZoneInfo(Local()));

    if (!zoneFileExists(id))
        throw std::invalid_argument("Time zone not found: " + id);

    // Use TZ env + tzset() to load the zone; protect with mutex (global env is not thread-safe)
    long   offset_secs = 0;
    bool   hasDst      = false;
    std::string abbrev = id;
    {
        std::lock_guard<std::mutex> lock(s_tzMutex);
        const char* saved = getenv("TZ");
        std::string savedStr = saved ? saved : "";
        setenv("TZ", id.c_str(), 1);
        tzset();
        time_t t = time(nullptr);
        struct tm tm_buf {};
        localtime_r(&t, &tm_buf);
        offset_secs = tm_buf.tm_gmtoff;
        hasDst      = (tm_buf.tm_isdst > 0);
        abbrev      = tm_buf.tm_zone ? tm_buf.tm_zone : id;
        if (!savedStr.empty()) setenv("TZ", savedStr.c_str(), 1);
        else                   unsetenv("TZ");
        tzset();
    }

    TimeSpan offset = TimeSpan::FromSeconds(static_cast<double>(offset_secs));
    return std::shared_ptr<TimeZoneInfo>(
        new TimeZoneInfo(id, offset, id, abbrev, abbrev, hasDst));
}

// ---------------------------------------------------------------------------
// GetSystemTimeZones()
// ---------------------------------------------------------------------------

std::vector<std::shared_ptr<TimeZoneInfo>> TimeZoneInfo::GetSystemTimeZones() {
    return {
        std::shared_ptr<TimeZoneInfo>(new TimeZoneInfo(Utc())),
        std::shared_ptr<TimeZoneInfo>(new TimeZoneInfo(Local()))
    };
}

} // namespace System
