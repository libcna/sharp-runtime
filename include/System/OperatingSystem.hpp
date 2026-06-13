// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/PlatformID.hpp"
#include "System/Version.hpp"

namespace System {

    /// <summary>Represents information about the operating system, such as the platform and version.</summary>
    class OperatingSystem {
        PlatformID platform_;
        Version version_;
        std::string servicePack_;

    public:
        /// Constructs an OperatingSystem with the specified platform identifier and version.
        /// @param platform Platform identifier (e.g. PlatformID::Unix).
        /// @param version  OS version.
        OperatingSystem(PlatformID platform, const Version& version)
            : platform_(platform), version_(version) {}

        /// Returns the platform identifier for this operating system.
        [[nodiscard]] PlatformID getPlatformProperty() const { return platform_; }
        /// Returns the version of this operating system.
        [[nodiscard]] const Version& getVersionProperty() const { return version_; }
        /// Returns the service pack string (empty in this port).
        [[nodiscard]] const std::string& getServicePackProperty() const { return servicePack_; }

        /// Returns a human-readable version string such as "Unix 5.15.0".
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

        /// Returns true when compiled for Windows (_WIN32 or _WIN64 defined).
        static bool IsWindows() {
#if defined(_WIN32) || defined(_WIN64)
            return true;
#else
            return false;
#endif
        }

        /// Returns true when compiled for Linux (__linux__ defined).
        static bool IsLinux() {
#if defined(__linux__)
            return true;
#else
            return false;
#endif
        }

        /// Returns true when compiled for macOS (__APPLE__ defined).
        static bool IsMacOS() {
#if defined(__APPLE__)
            return true;
#else
            return false;
#endif
        }

        /// Returns true when compiled for FreeBSD (__FreeBSD__ defined).
        static bool IsFreeBSD() {
#if defined(__FreeBSD__)
            return true;
#else
            return false;
#endif
        }

        /// Always returns false; Android is not supported in this port.
        static bool IsAndroid() { return false; }
        /// Always returns false; iOS is not supported in this port.
        static bool IsIOS()     { return false; }
        /// Always returns false; browser (Emscripten runtime detection) is not supported in this port.
        static bool IsBrowser() { return false; }

        /// Returns the same string as getVersionStringProperty().
        [[nodiscard]] std::string ToString() const { return getVersionStringProperty(); }
    };

} // namespace System
