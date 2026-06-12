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

        enum class SpecialFolder {
            Desktop                  = 0x0000,
            Programs                 = 0x0002,
            Personal                 = 0x0005,
            MyDocuments              = 0x0005,
            Favorites                = 0x0006,
            Startup                  = 0x0007,
            Recent                   = 0x0008,
            SendTo                   = 0x0009,
            StartMenu                = 0x000B,
            MyMusic                  = 0x000D,
            MyVideos                 = 0x000E,
            DesktopDirectory         = 0x0010,
            MyComputer               = 0x0011,
            NetworkShortcuts         = 0x0013,
            Fonts                    = 0x0014,
            Templates                = 0x0015,
            CommonStartMenu          = 0x0016,
            CommonPrograms           = 0x0017,
            CommonStartup            = 0x0018,
            CommonDesktopDirectory   = 0x0019,
            ApplicationData          = 0x001A,
            PrinterShortcuts         = 0x001B,
            LocalApplicationData     = 0x001C,
            InternetCache            = 0x0020,
            Cookies                  = 0x0021,
            History                  = 0x0022,
            CommonApplicationData    = 0x0023,
            Windows                  = 0x0024,
            System                   = 0x0025,
            ProgramFiles             = 0x0026,
            MyPictures               = 0x0027,
            UserProfile              = 0x0028,
            SystemX86                = 0x0029,
            ProgramFilesX86          = 0x002A,
            CommonProgramFiles       = 0x002B,
            CommonProgramFilesX86    = 0x002C,
            CommonTemplates          = 0x002D,
            CommonDocuments          = 0x002E,
            CommonAdminTools         = 0x002F,
            AdminTools               = 0x0030,
            CommonMusic              = 0x0035,
            CommonPictures           = 0x0036,
            CommonVideos             = 0x0037,
            Resources                = 0x0038,
            LocalizedResources       = 0x0039,
            CommonOemLinks           = 0x003A,
            CDBurning                = 0x003B,
        };

        enum class SpecialFolderOption {
            None        = 0,
            DoNotVerify = 0x4000,
            Create      = 0x8000,
        };

#ifdef _WIN32
        static inline const std::string NewLine = "\r\n";
#else
        static inline const std::string NewLine = "\n";
#endif

        /// @brief Returns the current working directory (platform-specific implementation in .cpp).
        [[nodiscard]] static std::string GetCurrentDirectory();

        [[nodiscard]] static std::string GetEnvironmentVariable(const std::string& name) {
            const char* val = std::getenv(name.c_str());
            return val ? std::string(val) : std::string();
        }

        /// @brief Returns the number of logical processors (platform-specific implementation in .cpp).
        [[nodiscard]] static SharpRuntime::intcs getProcessorCountProperty();

        static void Exit(SharpRuntime::intcs exitCode) { std::exit(exitCode); }
        static void FailFast(const std::string&)       { std::abort(); }

        [[nodiscard]] static bool Is64BitProcess() { return sizeof(void*) == 8; }
        [[nodiscard]] static bool Is64BitOperatingSystem() { return Is64BitProcess(); }
    };

} // namespace System
