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
#elif defined(__EMSCRIPTEN__)
// Emscripten: POSIX-like; <unistd.h> available
#  include <unistd.h>
#else
#  include <unistd.h>
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

} // namespace System
