// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/AppDomain.hpp"

// AppContext.hpp includes AppDomain.hpp for BaseDirectory, so the data-store
// forwarding below has to live here rather than in the header.
#include "System/AppContext.hpp"

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__EMSCRIPTEN__)
// No executable path — use virtual FS root
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#  include <climits>
#else
// Linux and other POSIX
#  include <climits>
#  include <unistd.h>
#endif

namespace System {

AppDomain::AppDomain() {
#if defined(_WIN32)
    wchar_t wbuf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, wbuf, MAX_PATH);
    if (len > 0) {
        int sz = WideCharToMultiByte(CP_UTF8, 0, wbuf, static_cast<int>(len),
                                     nullptr, 0, nullptr, nullptr);
        std::string path(static_cast<size_t>(sz), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wbuf, static_cast<int>(len),
                            path.data(), sz, nullptr, nullptr);
        auto pos = path.rfind('\\');
        if (pos == std::string::npos) pos = path.rfind('/');
        baseDirectory_ = (pos != std::string::npos) ? path.substr(0, pos + 1) : "./";
    } else {
        baseDirectory_ = "./";
    }
#elif defined(__EMSCRIPTEN__)
    baseDirectory_ = "./";
#elif defined(__APPLE__)
    char buf[PATH_MAX];
    uint32_t sz = sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) == 0) {
        std::string path(buf);
        auto pos = path.rfind('/');
        baseDirectory_ = (pos != std::string::npos) ? path.substr(0, pos + 1) : "./";
    } else {
        baseDirectory_ = "./";
    }
#else
    // Linux: /proc/self/exe
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::string path(buf);
        auto pos = path.rfind('/');
        baseDirectory_ = (pos != std::string::npos) ? path.substr(0, pos + 1) : "./";
    } else {
        baseDirectory_ = "./";
    }
#endif
}

// #2250 / SR-AUD-103. Transcribed from .NET's AppDomain.cs:171-174:
//     return AppContext.TryGetSwitch(value, out bool result) ? result : default(bool?);
//
// It used to `return false` unconditionally without consulting the switch registry at all, so a
// switch a caller had explicitly SET TO TRUE still reported as unset. Out of line for the same
// reason SetData/GetData below are: AppContext.hpp includes AppDomain.hpp for BaseDirectory.
std::optional<bool> AppDomain::IsCompatibilitySwitchSet(const std::string& value) const {
    bool result = false;
    if (AppContext::TryGetSwitch(value, result)) return result;
    return std::nullopt;
}

void AppDomain::SetData(const std::string& name, std::any data) {
    AppContext::SetData(name, std::move(data));
}

std::any AppDomain::GetData(const std::string& name) {
    return AppContext::GetData(name);
}

} // namespace System
