// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Environment.hpp"

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  undef GetCurrentDirectory   // windows.h macro collides with our method name
#elif defined(__EMSCRIPTEN__)
#  include <unistd.h>
#  include <climits>
#else
#  include <unistd.h>
#  include <climits>
#  include <pwd.h>
#  include <time.h>
#  include <sys/resource.h>
#endif
#include <sstream>

namespace System {

std::string Environment::GetCurrentDirectory() {
#if defined(_WIN32)
    char buf[4096];
    if (GetCurrentDirectoryA(static_cast<DWORD>(sizeof(buf)), buf))
        return std::string(buf);
    return "";
#else
    char buf[4096];
    if (getcwd(buf, sizeof(buf))) return std::string(buf);
    return "";
#endif
}

SharpRuntime::intcs Environment::getProcessorCountProperty() {
#if defined(_WIN32)
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    return static_cast<SharpRuntime::intcs>(si.dwNumberOfProcessors);
#elif defined(__EMSCRIPTEN__)
    return 1; // WebAssembly is single-threaded without pthreads
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? static_cast<SharpRuntime::intcs>(n) : 1;
#endif
}

std::string Environment::getMachineNameProperty() {
#if defined(_WIN32)
    char buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buf);
    if (GetComputerNameA(buf, &size)) return std::string(buf);
    return "";
#else
    char buf[HOST_NAME_MAX + 1];
    if (gethostname(buf, sizeof(buf)) == 0) return std::string(buf);
    return "";
#endif
}

std::string Environment::getUserNameProperty() {
#if defined(_WIN32)
    char buf[256];
    DWORD size = sizeof(buf);
    if (GetUserNameA(buf, &size)) return std::string(buf);
    return "";
#elif defined(__EMSCRIPTEN__)
    const char* user = std::getenv("USER");
    return user ? std::string(user) : std::string("user");
#else
    const char* user = std::getenv("USER");
    if (user) return std::string(user);
    struct passwd* pw = getpwuid(getuid());
    return pw ? std::string(pw->pw_name) : std::string();
#endif
}

void Environment::SetEnvironmentVariable(const std::string& name, const std::string& value) {
#if defined(_WIN32)
    if (value.empty())
        _putenv_s(name.c_str(), "");
    else
        _putenv_s(name.c_str(), value.c_str());
#else
    if (value.empty())
        ::unsetenv(name.c_str());
    else
        ::setenv(name.c_str(), value.c_str(), 1);
#endif
}

std::string Environment::ExpandEnvironmentVariables(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    size_t i = 0;
    while (i < name.size()) {
        if (name[i] == '%') {
            size_t end = name.find('%', i + 1);
            if (end != std::string::npos && end > i + 1) {
                std::string var = name.substr(i + 1, end - i - 1);
                const char* val = std::getenv(var.c_str());
                if (val) result += val;
                else { result += '%'; result += var; result += '%'; }
                i = end + 1;
                continue;
            }
        }
        result += name[i++];
    }
    return result;
}

std::string Environment::GetFolderPath(SpecialFolder folder) {
#if defined(_WIN32)
    char buf[MAX_PATH];
    if (SHGetFolderPathA(nullptr, static_cast<int>(folder), nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
        return std::string(buf);
    return "";
#else
    const char* home = std::getenv("HOME");
    std::string h = home ? std::string(home) : std::string();
    switch (folder) {
        case SpecialFolder::Personal:          // == MyDocuments (0x0005), returns home
        case SpecialFolder::UserProfile:      return h;
        case SpecialFolder::Desktop:
        case SpecialFolder::DesktopDirectory: return h + "/Desktop";
        case SpecialFolder::MyMusic:          return h + "/Music";
        case SpecialFolder::MyPictures:       return h + "/Pictures";
        case SpecialFolder::MyVideos:         return h + "/Videos";
        case SpecialFolder::ApplicationData:  return h + "/.config";
        case SpecialFolder::LocalApplicationData: return h + "/.local/share";
        case SpecialFolder::CommonApplicationData: return "/etc";
        case SpecialFolder::ProgramFiles:     return "/usr";
        case SpecialFolder::System:           return "/usr/lib";
        case SpecialFolder::Fonts:            return "/usr/share/fonts";
        case SpecialFolder::Templates:        return h + "/Templates";
        default:                              return "";
    }
#endif
}

Environment::ProcessCpuUsage Environment::getCpuUsageProperty() {
#if defined(_WIN32)
    FILETIME creation, exit, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        auto toTS = [](FILETIME ft) -> TimeSpan {
            longcs ticks = (static_cast<longcs>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
            return TimeSpan(ticks); // FILETIME is in 100-ns units, same as TimeSpan ticks
        };
        return { toTS(user), toTS(kernel) };
    }
    return { TimeSpan::Zero, TimeSpan::Zero };
#elif defined(__EMSCRIPTEN__)
    return { TimeSpan::Zero, TimeSpan::Zero };
#else
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        auto tvToTS = [](const struct timeval& tv) -> TimeSpan {
            longcs ticks = static_cast<longcs>(tv.tv_sec) * 10000000LL
                         + static_cast<longcs>(tv.tv_usec) * 10LL;
            return TimeSpan(ticks);
        };
        return { tvToTS(usage.ru_utime), tvToTS(usage.ru_stime) };
    }
    return { TimeSpan::Zero, TimeSpan::Zero };
#endif
}

SharpRuntime::intcs Environment::getProcessIdProperty() {
#if defined(_WIN32)
    return static_cast<SharpRuntime::intcs>(GetCurrentProcessId());
#else
    return static_cast<SharpRuntime::intcs>(::getpid());
#endif
}

SharpRuntime::longcs Environment::getTickCount64Property() {
#if defined(_WIN32)
    return static_cast<SharpRuntime::longcs>(GetTickCount64());
#else
    struct timespec ts{};
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return static_cast<SharpRuntime::longcs>(ts.tv_sec) * 1000LL
         + static_cast<SharpRuntime::longcs>(ts.tv_nsec) / 1000000LL;
#endif
}

} // namespace System
