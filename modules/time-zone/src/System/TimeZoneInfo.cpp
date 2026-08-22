// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/TimeZoneInfo.hpp"
#include "System/PlatformNotSupportedException.hpp"
#include "System/TimeZoneNotFoundException.hpp"
#include "TimeZonePosixSupport.hpp"
#include "System/InvalidTimeZoneException.hpp"
#include <cstdlib>
#include <ctime>

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__EMSCRIPTEN__)
// No timezone database available
#else
#  include <cstdio>
#  include <cstring>
#  include <sys/stat.h>
#endif

namespace System {

namespace {

#if defined(_WIN32)
std::string wideZoneNameOrFallback(const WCHAR* value, const std::string& fallback) {
    const int required =
        WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1) return fallback;

    std::string converted(static_cast<std::size_t>(required), '\0');
    const int written = WideCharToMultiByte(CP_UTF8, 0, value, -1, converted.data(), required,
                                            nullptr, nullptr);
    if (written != required) return fallback;
    converted.pop_back(); // WideCharToMultiByte included the terminator in `required`.
    return converted;
}
#endif

// TimeZoneInfo.Utc has process lifetime and .NET returns that exact object from UTC lookup and
// from the system-zone collection. Preserve that pointer identity without changing this port's
// shared_ptr-based lookup API or the pinned TimeZoneInfo object layout. Local deliberately does
// not use this helper: .NET clones Local specifically to break reference equality with the
// equivalent database zone.
std::shared_ptr<TimeZoneInfo> borrowStaticZone(const TimeZoneInfo& zone) {
    return std::shared_ptr<TimeZoneInfo>(const_cast<TimeZoneInfo*>(&zone),
                                         [](TimeZoneInfo*) noexcept {});
}

} // namespace

// ---------------------------------------------------------------------------
// Local()
// ---------------------------------------------------------------------------

const TimeZoneInfo& TimeZoneInfo::Local() {
    static TimeZoneInfo tz = []() -> TimeZoneInfo {
#if defined(_WIN32)
        DYNAMIC_TIME_ZONE_INFORMATION tzi{};
        const DWORD result = GetDynamicTimeZoneInformation(&tzi);
        if (result == TIME_ZONE_ID_INVALID)
            return TimeZoneInfo("Local", TimeSpan::Zero, "Local", "Local", "Local", false);
        // Bias values are minutes west of UTC. BaseUtcOffset is the STANDARD offset, not the
        // offset active today, and SupportsDaylightSavingTime describes the zone's rules rather
        // than whether today's date happens to fall in daylight time.
        const long offset_secs =
            -static_cast<long>(tzi.Bias + tzi.StandardBias) * 60L;
        const bool hasDst = !tzi.DynamicDaylightTimeDisabled &&
                            tzi.StandardDate.wMonth != 0 && tzi.DaylightDate.wMonth != 0;
        const std::string standardName = wideZoneNameOrFallback(tzi.StandardName, "Local");
        const std::string daylightName = wideZoneNameOrFallback(tzi.DaylightName, standardName);
        TimeSpan offset = TimeSpan::FromSeconds(static_cast<double>(offset_secs));
        return TimeZoneInfo("Local", offset, standardName, standardName, daylightName, hasDst);
#elif defined(__EMSCRIPTEN__)
        return TimeZoneInfo("Local", TimeSpan::Zero, "Local", "Local", "Local", false);
#else
        std::lock_guard<std::mutex> lock(detail::processTimeZoneMutex());
        detail::ZoneMetadata meta = detail::describeSelectedZone(detail::currentUtcYear());
        std::string standard = meta.standardAbbreviation.empty() ? std::string("Local")
                                                                 : meta.standardAbbreviation;
        std::string daylight = meta.daylightAbbreviation.empty() ? standard
                                                                 : meta.daylightAbbreviation;
        TimeSpan offset = TimeSpan::FromSeconds(static_cast<double>(meta.standardOffsetSeconds));
        return TimeZoneInfo("Local", offset, standard, standard, daylight,
                            meta.observesDaylight);
#endif
    }();
    return tz;
}

// ---------------------------------------------------------------------------
// FindSystemTimeZoneById()
// ---------------------------------------------------------------------------

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
namespace {

// Rejects an identifier that is not a relative, NUL-free, dot-free path of non-empty segments.
// An identifier reaches the filesystem and then setenv("TZ", ...), so the shapes rejected here
// are the ones that either escape /usr/share/zoneinfo or name a real zone by a spelling that
// is not its identifier: "" and " " (no segment), a leading '/', ".." and "." segments, an
// empty segment ("America//New_York"), a trailing '/', and an embedded NUL -- which is the
// worst of them, because the C string handed to stat() and setenv() stops at the NUL while the
// std::string stored as the zone's Id keeps every byte after it.
bool isWellFormedZoneId(const std::string& id) {
    if (id.empty()) return false;
    if (id.front() == '/') return false;
    if (id.find('\0') != std::string::npos) return false;
    std::size_t start = 0;
    while (true) {
        std::size_t slash = id.find('/', start);
        std::string segment = id.substr(start, slash == std::string::npos ? std::string::npos
                                                                          : slash - start);
        if (segment.empty() || segment == "." || segment == "..") return false;
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

// True when the file begins with the four bytes every version of the TZif format starts with.
// stat()+S_ISREG alone is not enough: /usr/share/zoneinfo ships plain-text data files
// (zone.tab, zone1970.tab, iso3166.tab, tzdata.zi, leapseconds, leap-seconds.list -- six of
// them here) that passed the old check, and glibc then parsed the identifier itself as a POSIX
// rule string, so every one of them resolved to a zone with offset 0 and a name taken from the
// filename. Prefixing the TZ value with ':' to force file interpretation was measured and does
// not change that, so the check has to happen here, at the door.
bool hasTzifMagic(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char magic[4] = {};
    std::size_t read = std::fread(magic, 1, sizeof(magic), f);
    std::fclose(f);
    return read == sizeof(magic) && std::memcmp(magic, "TZif", sizeof(magic)) == 0;
}

} // namespace

// Ticket #2186, question 4. #2183 folded three distinct failures into one boolean and kept
// TimeZoneNotFoundException for all of them "rather than guessing InvalidTimeZoneException".
// The guess is unnecessary now: .NET raises TimeZoneNotFoundException when the id names nothing,
// and InvalidTimeZoneException when the file EXISTS but is not usable zone data
// (`TimeZoneInfo.Unix.cs:697`, SR.InvalidTimeZone_NoTTInfoStructures). The two are different
// answers to different questions -- "there is no such zone" versus "that is not a zone" -- and a
// caller that catches only the first would previously have swallowed the second.
//
// TryFindSystemTimeZoneById is unaffected: .NET's discards the exception entirely
// (`TimeZoneInfo.cs:526-527`), so it returns false for every failure, and this port's `catch (...)`
// already did the same.
static bool zoneFileExists(const std::string& id) {
    if (!isWellFormedZoneId(id)) return false;
    std::string path = "/usr/share/zoneinfo/" + id;
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

// True when a file that EXISTS does not carry zone data. Separated from zoneFileExists so the
// caller can tell the two failures apart; see the comment above.
static bool zoneFileIsNotZoneData(const std::string& id) {
    return !hasTzifMagic("/usr/share/zoneinfo/" + id);
}
#endif

// ---------------------------------------------------------------------------
// IANA ↔ Windows timezone name mapping (CLDR-derived, ~85 common zones)
// Always compiled; used by TryConvertIanaIdToWindowsId / TryConvertWindowsIdToIanaId
// and, on Windows, by FindSystemTimeZoneById.
// ---------------------------------------------------------------------------
namespace {
static const struct { const char* iana; const char* win; } kIanaWindowsMap[] = {
        {"Africa/Abidjan",                  "Greenwich Standard Time"},
        {"Africa/Accra",                    "Greenwich Standard Time"},
        {"Africa/Cairo",                    "Egypt Standard Time"},
        {"Africa/Johannesburg",             "South Africa Standard Time"},
        {"Africa/Lagos",                    "W. Central Africa Standard Time"},
        {"Africa/Nairobi",                  "E. Africa Standard Time"},
        {"America/Anchorage",               "Alaskan Standard Time"},
        {"America/Argentina/Buenos_Aires",  "Argentina Standard Time"},
        {"America/Bogota",                  "SA Pacific Standard Time"},
        {"America/Chicago",                 "Central Standard Time"},
        {"America/Denver",                  "Mountain Standard Time"},
        {"America/Halifax",                 "Atlantic Standard Time"},
        {"America/Lima",                    "SA Pacific Standard Time"},
        {"America/Los_Angeles",             "Pacific Standard Time"},
        {"America/Mexico_City",             "Central Standard Time (Mexico)"},
        {"America/New_York",                "Eastern Standard Time"},
        {"America/Phoenix",                 "US Mountain Standard Time"},
        {"America/Regina",                  "Canada Central Standard Time"},
        {"America/Sao_Paulo",               "E. South America Standard Time"},
        {"America/St_Johns",                "Newfoundland Standard Time"},
        {"America/Toronto",                 "Eastern Standard Time"},
        {"America/Vancouver",               "Pacific Standard Time"},
        {"Asia/Almaty",                     "Central Asia Standard Time"},
        {"Asia/Baghdad",                    "Arabic Standard Time"},
        {"Asia/Bangkok",                    "SE Asia Standard Time"},
        {"Asia/Calcutta",                   "India Standard Time"},
        {"Asia/Colombo",                    "Sri Lanka Standard Time"},
        {"Asia/Dubai",                      "Arabian Standard Time"},
        {"Asia/Hong_Kong",                  "China Standard Time"},
        {"Asia/Jakarta",                    "SE Asia Standard Time"},
        {"Asia/Jerusalem",                  "Israel Standard Time"},
        {"Asia/Kabul",                      "Afghanistan Standard Time"},
        {"Asia/Karachi",                    "Pakistan Standard Time"},
        {"Asia/Kathmandu",                  "Nepal Standard Time"},
        {"Asia/Kolkata",                    "India Standard Time"},
        {"Asia/Krasnoyarsk",                "North Asia Standard Time"},
        {"Asia/Kuala_Lumpur",               "Singapore Standard Time"},
        {"Asia/Kuwait",                     "Arab Standard Time"},
        {"Asia/Muscat",                     "Arabian Standard Time"},
        {"Asia/Novosibirsk",                "N. Central Asia Standard Time"},
        {"Asia/Rangoon",                    "Myanmar Standard Time"},
        {"Asia/Riyadh",                     "Arab Standard Time"},
        {"Asia/Seoul",                      "Korea Standard Time"},
        {"Asia/Shanghai",                   "China Standard Time"},
        {"Asia/Singapore",                  "Singapore Standard Time"},
        {"Asia/Taipei",                     "Taipei Standard Time"},
        {"Asia/Tashkent",                   "West Asia Standard Time"},
        {"Asia/Tehran",                     "Iran Standard Time"},
        {"Asia/Tokyo",                      "Tokyo Standard Time"},
        {"Asia/Ulaanbaatar",                "Ulaanbaatar Standard Time"},
        {"Asia/Vladivostok",                "Vladivostok Standard Time"},
        {"Asia/Yakutsk",                    "Yakutsk Standard Time"},
        {"Asia/Yekaterinburg",              "Ekaterinburg Standard Time"},
        {"Atlantic/Azores",                 "Azores Standard Time"},
        {"Atlantic/Cape_Verde",             "Cape Verde Standard Time"},
        {"Australia/Adelaide",              "Cen. Australia Standard Time"},
        {"Australia/Brisbane",              "E. Australia Standard Time"},
        {"Australia/Darwin",                "AUS Central Standard Time"},
        {"Australia/Hobart",                "Tasmania Standard Time"},
        {"Australia/Perth",                 "W. Australia Standard Time"},
        {"Australia/Sydney",                "AUS Eastern Standard Time"},
        {"Europe/Amsterdam",                "W. Europe Standard Time"},
        {"Europe/Athens",                   "GTB Standard Time"},
        {"Europe/Berlin",                   "W. Europe Standard Time"},
        {"Europe/Brussels",                 "Romance Standard Time"},
        {"Europe/Bucharest",                "GTB Standard Time"},
        {"Europe/Budapest",                 "Central Europe Standard Time"},
        {"Europe/Dublin",                   "GMT Standard Time"},
        {"Europe/Helsinki",                 "FLE Standard Time"},
        {"Europe/Istanbul",                 "Turkey Standard Time"},
        {"Europe/Kiev",                     "FLE Standard Time"},
        {"Europe/Lisbon",                   "GMT Standard Time"},
        {"Europe/London",                   "GMT Standard Time"},
        {"Europe/Madrid",                   "Romance Standard Time"},
        {"Europe/Minsk",                    "Belarus Standard Time"},
        {"Europe/Moscow",                   "Russian Standard Time"},
        {"Europe/Paris",                    "Romance Standard Time"},
        {"Europe/Prague",                   "Central Europe Standard Time"},
        {"Europe/Rome",                     "W. Europe Standard Time"},
        {"Europe/Sofia",                    "FLE Standard Time"},
        {"Europe/Stockholm",                "W. Europe Standard Time"},
        {"Europe/Vienna",                   "W. Europe Standard Time"},
        {"Europe/Warsaw",                   "Central European Standard Time"},
        {"Europe/Zurich",                   "W. Europe Standard Time"},
        {"Pacific/Auckland",                "New Zealand Standard Time"},
        {"Pacific/Fiji",                    "Fiji Standard Time"},
        {"Pacific/Guam",                    "West Pacific Standard Time"},
        {"Pacific/Honolulu",                "Hawaiian Standard Time"},
        {"Pacific/Midway",                  "Samoa Standard Time"},
        {"Pacific/Port_Moresby",            "West Pacific Standard Time"},
        {"Pacific/Tongatapu",               "Tonga Standard Time"},
    {nullptr, nullptr}
};

static const char* ianaToWindows(const std::string& iana) {
    for (const auto* p = kIanaWindowsMap; p->iana; ++p)
        if (iana == p->iana) return p->win;
    return nullptr;
}

static const char* windowsToIana(const std::string& win) {
    for (const auto* p = kIanaWindowsMap; p->iana; ++p)
        if (win == p->win) return p->iana;
    return nullptr;
}
} // anonymous namespace

std::shared_ptr<TimeZoneInfo> TimeZoneInfo::FindSystemTimeZoneById(const std::string& id) {
    // .NET reserves UTC through an OrdinalIgnoreCase comparison before platform lookup. Keep the
    // fold local and ASCII/locale-independent, matching zone-id equality elsewhere in this type.
    if (foldZoneId(id) == "utc") return borrowStaticZone(Utc());
    if (foldZoneId(id) == "local")
        return std::shared_ptr<TimeZoneInfo>(new TimeZoneInfo(Local()));

#if defined(_WIN32)
    const char* winName = ianaToWindows(id);
    if (!winName)
        throw System::TimeZoneNotFoundException(
            "The time zone ID '" + id + "' was not found on the local computer.");

    DYNAMIC_TIME_ZONE_INFORMATION dtzi{};
    for (DWORD i = 0; EnumDynamicTimeZoneInformation(i, &dtzi) != ERROR_NO_MORE_ITEMS; ++i) {
        char tzKey[128] = {};
        WideCharToMultiByte(CP_UTF8, 0, dtzi.TimeZoneKeyName, -1, tzKey, sizeof(tzKey), nullptr, nullptr);
        if (std::string(tzKey) != winName) continue;

        SYSTEMTIME st{};
        GetSystemTime(&st);
        TIME_ZONE_INFORMATION tzi{};
        if (GetTimeZoneInformationForYear(st.wYear, &dtzi, &tzi) == FALSE)
            throw System::InvalidTimeZoneException(
                "The time zone data for '" + id + "' could not be read.");
        const long offset_secs = -(tzi.Bias + tzi.StandardBias) * 60L;
        const bool hasDst = dtzi.DynamicDaylightTimeDisabled == FALSE &&
                            tzi.StandardDate.wMonth != 0 && tzi.DaylightDate.wMonth != 0;
        char stdName[64] = {};
        char dstName[64] = {};
        WideCharToMultiByte(CP_UTF8, 0, tzi.StandardName, -1, stdName, sizeof(stdName), nullptr, nullptr);
        WideCharToMultiByte(CP_UTF8, 0, tzi.DaylightName, -1, dstName, sizeof(dstName), nullptr, nullptr);
        TimeSpan offset = TimeSpan::FromSeconds(static_cast<double>(offset_secs));
        return std::shared_ptr<TimeZoneInfo>(
            new TimeZoneInfo(id, offset, id, stdName, dstName, hasDst));
    }
    throw System::TimeZoneNotFoundException(
        "The time zone ID '" + id + "' was not found on the local computer.");
#elif defined(__EMSCRIPTEN__)
    throw System::PlatformNotSupportedException(
        "FindSystemTimeZoneById is not supported on Emscripten.");
#else
    if (!zoneFileExists(id))
        throw System::TimeZoneNotFoundException(
            "The time zone ID '" + id + "' was not found on the local computer.");
    if (zoneFileIsNotZoneData(id))
        throw System::InvalidTimeZoneException(
            "There are no ttinfo structures in the tzfile.  At least one ttinfo structure is "
            "required in order to construct a TimeZoneInfo object.");

    detail::ZoneMetadata meta;
    {
        std::lock_guard<std::mutex> lock(detail::processTimeZoneMutex());
        detail::ScopedTz restoreTz;      // restores TZ on every path, including an exception
        restoreTz.select(id);
        meta = detail::describeSelectedZone(detail::currentUtcYear());
    }
    std::string standard = meta.standardAbbreviation.empty() ? id : meta.standardAbbreviation;
    std::string daylight = meta.daylightAbbreviation.empty() ? standard
                                                             : meta.daylightAbbreviation;
    TimeSpan offset = TimeSpan::FromSeconds(static_cast<double>(meta.standardOffsetSeconds));
    return std::shared_ptr<TimeZoneInfo>(
        new TimeZoneInfo(id, offset, id, standard, daylight, meta.observesDaylight));
#endif
}

// ---------------------------------------------------------------------------
// GetSystemTimeZones()
// ---------------------------------------------------------------------------

std::vector<std::shared_ptr<TimeZoneInfo>> TimeZoneInfo::GetSystemTimeZones() {
    return {
        borrowStaticZone(Utc()),
        std::shared_ptr<TimeZoneInfo>(new TimeZoneInfo(Local()))
    };
}

bool TimeZoneInfo::TryConvertIanaIdToWindowsId(const std::string& ianaId, std::string& windowsId) {
    const char* win = ianaToWindows(ianaId);
    if (!win) return false;
    windowsId = win;
    return true;
}

bool TimeZoneInfo::TryConvertWindowsIdToIanaId(const std::string& windowsId, std::string& ianaId) {
    const char* iana = windowsToIana(windowsId);
    if (!iana) return false;
    ianaId = iana;
    return true;
}

} // namespace System
