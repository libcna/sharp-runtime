// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/PlatformID.hpp"
#include "System/Version.hpp"

namespace System {

    class OperatingSystem {
        PlatformID platform_;
        Version version_;
        std::string servicePack_;

    public:
        OperatingSystem(PlatformID platform, const Version& version)
            : platform_(platform), version_(version) {}

        [[nodiscard]] PlatformID getPlatformProperty() const { return platform_; }
        [[nodiscard]] const Version& getVersionProperty() const { return version_; }
        [[nodiscard]] const std::string& getServicePackProperty() const { return servicePack_; }

        [[nodiscard]] std::string getVersionStringProperty() const {
            std::string name;
            switch (platform_) {
                case PlatformID::Win32NT:      name = "Microsoft Windows NT "; break;
                case PlatformID::Unix:         name = "Unix "; break;
                case PlatformID::MacOSX:       name = "Mac OS X "; break;
                default:                       name = "Unknown "; break;
            }
            return name + version_.ToString();
        }

        // Static helpers matching .NET 6+ OperatingSystem.IsXxx() API
        static bool IsWindows() {
#if defined(_WIN32) || defined(_WIN64)
            return true;
#else
            return false;
#endif
        }

        static bool IsLinux() {
#if defined(__linux__)
            return true;
#else
            return false;
#endif
        }

        static bool IsMacOS() {
#if defined(__APPLE__)
            return true;
#else
            return false;
#endif
        }

        static bool IsFreeBSD() {
#if defined(__FreeBSD__)
            return true;
#else
            return false;
#endif
        }

        static bool IsAndroid() { return false; }
        static bool IsIOS()     { return false; }
        static bool IsBrowser() { return false; }

        [[nodiscard]] std::string ToString() const { return getVersionStringProperty(); }
    };

} // namespace System
