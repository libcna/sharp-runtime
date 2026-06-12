// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/TimeZoneInfo.hpp"
#include "System/PlatformNotSupportedException.hpp"
#include <cstdlib>
#include <ctime>
#include <mutex>

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__EMSCRIPTEN__)
// No timezone database available
#else
#  include <sys/stat.h>
#endif

namespace System {

// ---------------------------------------------------------------------------
// Local()
// ---------------------------------------------------------------------------

const TimeZoneInfo& TimeZoneInfo::Local() {
    static TimeZoneInfo tz = []() -> TimeZoneInfo {
#if defined(_WIN32)
        TIME_ZONE_INFORMATION tzi{};
        DWORD result = GetTimeZoneInformation(&tzi);
        // Bias is minutes west of UTC; negative for east
        long offset_secs = -(tzi.Bias * 60L);
        bool hasDst = (result == TIME_ZONE_ID_DAYLIGHT);
        // Convert StandardName (WCHAR) to narrow string
        char name[64] = "Local";
        WideCharToMultiByte(CP_UTF8, 0, tzi.StandardName, -1, name, sizeof(name), nullptr, nullptr);
        TimeSpan offset = TimeSpan::FromSeconds(static_cast<double>(offset_secs));
        return TimeZoneInfo("Local", offset, name, name, name, hasDst);
#elif defined(__EMSCRIPTEN__)
        return TimeZoneInfo::Utc();
#else
        time_t t = time(nullptr);
        struct tm local_tm {};
        localtime_r(&t, &local_tm);
        long   offset_secs = local_tm.tm_gmtoff;
        bool   hasDst      = (local_tm.tm_isdst > 0);
        std::string name   = local_tm.tm_zone ? local_tm.tm_zone : "Local";
        TimeSpan offset    = TimeSpan::FromSeconds(static_cast<double>(offset_secs));
        return TimeZoneInfo("Local", offset, name, name, name, hasDst);
#endif
    }();
    return tz;
}

// ---------------------------------------------------------------------------
// FindSystemTimeZoneById()
// ---------------------------------------------------------------------------

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
static bool zoneFileExists(const std::string& id) {
    if (id.find("..") != std::string::npos) return false;
    std::string path = "/usr/share/zoneinfo/" + id;
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
static std::mutex s_tzMutex;
#endif

std::shared_ptr<TimeZoneInfo> TimeZoneInfo::FindSystemTimeZoneById(const std::string& id) {
    if (id == "UTC")   return std::shared_ptr<TimeZoneInfo>(new TimeZoneInfo(Utc()));
    if (id == "Local") return std::shared_ptr<TimeZoneInfo>(new TimeZoneInfo(Local()));

#if defined(_WIN32)
    // Windows uses its own timezone names, not IANA IDs.
    // Only UTC/Local are supported without a third-party IANA→Windows mapping.
    throw System::PlatformNotSupportedException(
        "FindSystemTimeZoneById with IANA IDs is not supported on Windows. "
        "Use 'UTC' or 'Local'.");
#elif defined(__EMSCRIPTEN__)
    throw System::PlatformNotSupportedException(
        "FindSystemTimeZoneById is not supported on Emscripten.");
#else
    if (!zoneFileExists(id))
        throw std::invalid_argument("Time zone not found: " + id);

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
#endif
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
