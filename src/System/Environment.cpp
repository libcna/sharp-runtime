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
#endif

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
