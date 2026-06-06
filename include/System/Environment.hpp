// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <cstdlib>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    /**
     * @brief Provides information about, and means to manipulate, the current
     * environment and platform.
     *
     * Partial C++ counterpart of .NET System.Environment.
     *
     * @note Status: Partial
     */
    class Environment {
    public:
        Environment() = delete;

#ifdef _WIN32
        static inline const std::string NewLine = "\r\n";
#else
        static inline const std::string NewLine = "\n";
#endif

        [[nodiscard]] static std::string GetCurrentDirectory() {
            char buf[4096];
#ifdef _WIN32
            if (GetCurrentDirectoryA(sizeof(buf), buf)) return std::string(buf);
#else
            if (getcwd(buf, sizeof(buf))) return std::string(buf);
#endif
            return "";
        }

        [[nodiscard]] static std::string GetEnvironmentVariable(const std::string& name) {
            const char* val = std::getenv(name.c_str());
            return val ? std::string(val) : std::string();
        }

        [[nodiscard]] static SharpRuntime::intcs getProcessorCountProperty() {
#ifdef _WIN32
            SYSTEM_INFO si; GetSystemInfo(&si);
            return static_cast<SharpRuntime::intcs>(si.dwNumberOfProcessors);
#else
            long n = sysconf(_SC_NPROCESSORS_ONLN);
            return n > 0 ? static_cast<SharpRuntime::intcs>(n) : 1;
#endif
        }

        static void Exit(SharpRuntime::intcs exitCode) { std::exit(exitCode); }
        static void FailFast(const std::string&)       { std::abort(); }

        [[nodiscard]] static bool Is64BitProcess() { return sizeof(void*) == 8; }
        [[nodiscard]] static bool Is64BitOperatingSystem() { return Is64BitProcess(); }
    };

} // namespace System
