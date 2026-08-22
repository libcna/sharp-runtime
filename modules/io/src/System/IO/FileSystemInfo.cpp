// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/FileSystemInfo.hpp"
#include "System/IO/IOException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/TimeZone.hpp"

#include <chrono>
#include <ctime>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <sys/stat.h>
#else
#  include <sys/stat.h>
#endif

namespace System::IO {

    using SharpRuntime::longcs;

    namespace {

        System::DateTime fromUnixTime(longcs seconds) {
            constexpr longcs minSeconds =
                -System::DateTime::UnixEpochTicks / System::DateTime::TicksPerSecond;
            constexpr longcs maxSeconds =
                (System::DateTime::MaxTicks - System::DateTime::UnixEpochTicks) /
                System::DateTime::TicksPerSecond;
            if (seconds < minSeconds || seconds > maxSeconds) {
                throw System::ArgumentOutOfRangeException(
                    "seconds", "The filesystem timestamp is outside DateTime's range.");
            }
            // The bounds above make this multiplication and addition representable; without
            // them a malformed/FUSE-backed 64-bit stat timestamp could trigger signed-overflow
            // UB before DateTime's constructor had a chance to reject it.
            longcs ticks = System::DateTime::UnixEpochTicks +
                           seconds * System::DateTime::TicksPerSecond;
            return System::DateTime(ticks, System::DateTimeKind::Utc);
        }

        // C++20 [time.clock.file] lets an implementation give file_clock *either* to_sys/from_sys
        // *or* to_utc/from_utc, and both choices are conforming: libstdc++ and libc++ provide the
        // sys pair, Microsoft's STL provides the utc pair plus std::chrono::clock_cast, which
        // converts between whichever pair exists.
        //
        // This has to be a preprocessor split, not a `requires` detection or an if-constexpr
        // fallback. Both alternatives were measured on real CI runners and both are hard errors:
        // on Microsoft's STL `requires { file_clock::to_sys(ft); }` is a diagnosable error rather
        // than an unsatisfied requirement, because file_clock is not a dependent type there; and
        // Apple's libc++ does not declare std::chrono::clock_cast at all, so naming it even in a
        // discarded if-constexpr branch fails at definition time. Only text the preprocessor
        // removes is safe here. _MSVC_STL_VERSION identifies the library rather than the
        // compiler, which is what actually decides the member set.
#if defined(_MSVC_STL_VERSION)
        std::chrono::system_clock::time_point fileTimeToSystemTime(std::filesystem::file_time_type ft) {
            return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                std::chrono::clock_cast<std::chrono::system_clock>(ft));
        }

        std::filesystem::file_time_type systemTimeToFileTime(std::chrono::system_clock::time_point sys) {
            return std::chrono::clock_cast<std::chrono::file_clock>(sys);
        }
#else
        std::chrono::system_clock::time_point fileTimeToSystemTime(std::filesystem::file_time_type ft) {
            return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                std::chrono::file_clock::to_sys(ft));
        }

        std::filesystem::file_time_type systemTimeToFileTime(std::chrono::system_clock::time_point sys) {
            return std::chrono::file_clock::from_sys(sys);
        }
#endif

        System::DateTime fromFileClock(std::filesystem::file_time_type ft) {
            // The conversion may yield a finer-grained duration than system_clock::time_point
            // (e.g. nanoseconds on libc++/Emscripten vs. microseconds on libstdc++); to_time_t()
            // only accepts the exact system_clock::time_point type, so the helper casts.
            return fromUnixTime(
                static_cast<longcs>(std::chrono::system_clock::to_time_t(fileTimeToSystemTime(ft))));
        }

        struct RawTimes {
            longcs creation;
            longcs access;
        };

        RawTimes statTimes(const std::filesystem::path& path) {
#ifdef _WIN32
            struct _stat64 st{};
            if (_wstat64(path.wstring().c_str(), &st) != 0)
                throw IOException("Failed to stat '" + path.string() + "'.");
            // On Windows, st_ctime from _stat64 reflects file creation time.
            return { static_cast<longcs>(st.st_ctime), static_cast<longcs>(st.st_atime) };
#else
            struct stat st{};
            if (::stat(path.c_str(), &st) != 0)
                throw IOException("Failed to stat '" + path.string() + "'.");
            // POSIX has no portable birth-time field in <sys/stat.h>; st_ctime (inode
            // metadata change time) is the closest portable approximation, matching
            // real .NET's own documented Linux fallback behavior.
            return { static_cast<longcs>(st.st_ctime), static_cast<longcs>(st.st_atime) };
#endif
        }

    } // namespace

    System::DateTime FileSystemInfo::getCreationTimeUtcProperty() const {
        return fromUnixTime(statTimes(fullPath_).creation);
    }

    // Verified against FileSystemInfo.cs: CreationTime/LastAccessTime/LastWriteTime are all
    // `Xxx => XxxUtc.ToLocalTime()` (and the LastWriteTime setter is `XxxUtc = value.
    // ToUniversalTime()`). IO can reach the date-sensitive legacy TimeZone adapter, so using
    // TimeZoneInfo.Local's fixed standard offset here would be one hour wrong during DST.
    System::DateTime FileSystemInfo::getCreationTimeProperty() const {
        return getCreationTimeUtcProperty().ToLocalTime(System::TimeZone::CurrentTimeZone());
    }

    System::DateTime FileSystemInfo::getLastAccessTimeUtcProperty() const {
        return fromUnixTime(statTimes(fullPath_).access);
    }

    System::DateTime FileSystemInfo::getLastAccessTimeProperty() const {
        return getLastAccessTimeUtcProperty().ToLocalTime(System::TimeZone::CurrentTimeZone());
    }

    System::DateTime FileSystemInfo::getLastWriteTimeUtcProperty() const {
        std::error_code ec;
        auto ft = std::filesystem::last_write_time(fullPath_, ec);
        if (ec) throw IOException("Failed to get last write time of '" + fullPath_.string() + "': " + ec.message());
        return fromFileClock(ft);
    }

    System::DateTime FileSystemInfo::getLastWriteTimeProperty() const {
        return getLastWriteTimeUtcProperty().ToLocalTime(System::TimeZone::CurrentTimeZone());
    }

    void FileSystemInfo::setLastWriteTimeUtcProperty(const System::DateTime& value) {
        // File.GetUtcDateTimeOffset has an intentionally asymmetric contract: a Local value is
        // converted to UTC, while Utc and Unspecified values already describe the UTC clock
        // value to write.  Keeping that distinction here matters now that DateTime carries Kind;
        // feeding all three through the Local source zone would reject Utc and move Unspecified
        // twice at the public XxxUtc boundary.
        const System::DateTime utcValue =
            value.getKindProperty() == System::DateTimeKind::Local
                ? value.ToUniversalTime(System::TimeZone::CurrentTimeZone())
                : System::DateTime::SpecifyKind(value, System::DateTimeKind::Utc);
        const longcs unixTicks = utcValue.getTicksProperty() - System::DateTime::UnixEpochTicks;
        longcs unixSeconds = unixTicks / System::DateTime::TicksPerSecond;
        if (unixTicks % System::DateTime::TicksPerSecond < 0) --unixSeconds;
        auto sysTime = std::chrono::system_clock::from_time_t(static_cast<std::time_t>(unixSeconds));
        auto fileTime = systemTimeToFileTime(sysTime);
        std::error_code ec;
        std::filesystem::last_write_time(fullPath_, fileTime, ec);
        if (ec) throw IOException("Failed to set last write time of '" + fullPath_.string() + "': " + ec.message());
    }

    void FileSystemInfo::setLastWriteTimeProperty(const System::DateTime& value) {
        // DateTime.ToUniversalTime leaves a Utc value unchanged and treats Local/Unspecified as
        // local clock values. Spell that matrix out and use the date-sensitive legacy adapter,
        // which IO can reach without adding an implicit Core.Base timezone dependency.
        const System::DateTime utcValue =
            value.getKindProperty() == System::DateTimeKind::Utc
                ? value
                : value.ToUniversalTime(System::TimeZone::CurrentTimeZone());
        setLastWriteTimeUtcProperty(utcValue);
    }

} // namespace System::IO
