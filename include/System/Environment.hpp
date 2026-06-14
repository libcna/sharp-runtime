// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>
#include <cstdlib>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/EnvironmentVariableTarget.hpp"
#include "System/TimeSpan.hpp"

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

        /**
         * @brief Represents the amount of time the process has spent running in user mode and kernel mode.
         * C++ counterpart of .NET System.Environment.ProcessCpuUsage.
         */
        struct ProcessCpuUsage {
            /** @brief Gets the amount of time the process has spent running code in user mode. */
            TimeSpan UserTime;
            /** @brief Gets the amount of time the process has spent running code in kernel mode. */
            TimeSpan PrivilegedTime;
            /** @brief Gets the total CPU time (UserTime + PrivilegedTime). */
            [[nodiscard]] TimeSpan getTotalTimeProperty() const { return UserTime + PrivilegedTime; }
        };

        /// @brief Returns the CPU usage of the current process.
        [[nodiscard]] static ProcessCpuUsage getCpuUsageProperty();

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

        /// @brief Gets a value indicating whether the current process is shutting down. Always false in this implementation.
        static constexpr bool HasShutdownStarted = false;

        /// @brief Returns the value of an environment variable of the current process.
        [[nodiscard]] static std::string GetEnvironmentVariable(const std::string& name) {
            const char* val = std::getenv(name.c_str());
            return val ? std::string(val) : std::string();
        }

        /// @brief Returns the value of an environment variable from the specified target (Process only).
        [[nodiscard]] static std::string GetEnvironmentVariable(const std::string& name, EnvironmentVariableTarget) {
            return GetEnvironmentVariable(name);
        }

        /// @brief Sets an environment variable for the current process. Pass empty value to remove.
        static void SetEnvironmentVariable(const std::string& name, const std::string& value);

        /// @brief Replaces the name of each environment variable in @p name with its value.
        [[nodiscard]] static std::string ExpandEnvironmentVariables(const std::string& name);

        /// @brief Returns the path of the specified system special folder.
        [[nodiscard]] static std::string GetFolderPath(SpecialFolder folder);

        /// @brief Returns the path of the specified system special folder, with the given option.
        [[nodiscard]] static std::string GetFolderPath(SpecialFolder folder, SpecialFolderOption) {
            return GetFolderPath(folder);
        }

        /// @brief Returns the number of logical processors (platform-specific implementation in .cpp).
        [[nodiscard]] static SharpRuntime::intcs getProcessorCountProperty();

        /// @brief Returns the unique identifier of the current process.
        [[nodiscard]] static SharpRuntime::intcs getProcessIdProperty();

        /// @brief Returns the number of milliseconds elapsed since system startup (32-bit, may overflow).
        [[nodiscard]] static SharpRuntime::intcs getTickCountProperty() {
            return static_cast<SharpRuntime::intcs>(getTickCount64Property());
        }

        static void Exit(SharpRuntime::intcs exitCode) { std::exit(exitCode); }
        static void FailFast(const std::string&)       { std::abort(); }

        [[nodiscard]] static bool Is64BitProcess() { return sizeof(void*) == 8; }
        [[nodiscard]] static bool Is64BitOperatingSystem() { return Is64BitProcess(); }

        /// @brief Returns the NetBIOS name of the local machine.
        [[nodiscard]] static std::string getMachineNameProperty();

        /// @brief Returns the user name of the person currently logged on.
        [[nodiscard]] static std::string getUserNameProperty();

        /// @brief Returns the number of milliseconds elapsed since system startup (64-bit).
        [[nodiscard]] static SharpRuntime::longcs getTickCount64Property();
    };

} // namespace System
