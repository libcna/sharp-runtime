// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <cstdlib>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/EnvironmentVariableTarget.hpp"
#include "System/TimeSpan.hpp"

namespace System {

/**
 * @brief Provides information about, and means to manipulate, the current environment and platform.
 *
 * Partial C++ counterpart of .NET System.Environment.
 */
class Environment {
public:
    Environment() = delete;

    // -------------------------------------------------------------------------
    // Nested types
    // -------------------------------------------------------------------------

    /**
     * @brief Represents the CPU usage statistics of the current process.
     *
     * C++ counterpart of .NET System.Environment.ProcessCpuUsage.
     */
    struct ProcessCpuUsage {
        /** @brief Time the process has spent running code in user mode. */
        TimeSpan UserTime;
        /** @brief Time the process has spent running code in kernel (privileged) mode. */
        TimeSpan PrivilegedTime;
        /**
         * @brief Gets the total CPU time (UserTime + PrivilegedTime).
         * C++ counterpart of .NET Environment.ProcessCpuUsage.TotalTime.
         */
        [[nodiscard]] TimeSpan getTotalTimeProperty() const { return UserTime + PrivilegedTime; }
    };

    /**
     * @brief Specifies enumerated constants used to retrieve directory paths to
     * system special folders.
     *
     * C++ counterpart of .NET System.Environment.SpecialFolder.
     */
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

    /**
     * @brief Specifies options to use when getting the path to a special folder.
     *
     * C++ counterpart of .NET System.Environment.SpecialFolderOption.
     */
    enum class SpecialFolderOption {
        None        = 0,
        DoNotVerify = 0x4000,
        Create      = 0x8000,
    };

    // -------------------------------------------------------------------------
    // Newline
    // -------------------------------------------------------------------------

#ifdef _WIN32
    /** @brief Gets the newline string defined for this environment (CRLF on Windows, LF elsewhere). */
    static inline const std::string NewLine = "\r\n";
#else
    /** @brief Gets the newline string defined for this environment (CRLF on Windows, LF elsewhere). */
    static inline const std::string NewLine = "\n";
#endif

    // -------------------------------------------------------------------------
    // Process state
    // -------------------------------------------------------------------------

    /** @brief Gets a value indicating whether the current process is shutting down. Always false. */
    static constexpr bool HasShutdownStarted = false;

    /**
     * @brief Gets a value indicating whether the current process is privileged (root/Administrator).
     *
     * C++ counterpart of .NET Environment.IsPrivilegedProcess.
     * On POSIX returns true when euid == 0. On Windows checks token elevation.
     */
    [[nodiscard]] static bool getIsPrivilegedProcessProperty();

    // -------------------------------------------------------------------------
    // Current directory
    // -------------------------------------------------------------------------

    /**
     * @brief Gets the fully qualified path of the current working directory.
     *
     * C++ counterpart of the getter of .NET Environment.CurrentDirectory.
     */
    [[nodiscard]] static std::string GetCurrentDirectory();

    /**
     * @brief Sets the current working directory to the specified path.
     *
     * C++ counterpart of the setter of .NET Environment.CurrentDirectory.
     * @param path The path to the new current directory.
     */
    static void SetCurrentDirectory(const std::string& path);

    // -------------------------------------------------------------------------
    // Process identification & executable path
    // -------------------------------------------------------------------------

    /**
     * @brief Gets the unique identifier of the current process.
     *
     * C++ counterpart of .NET Environment.ProcessId.
     */
    [[nodiscard]] static SharpRuntime::intcs getProcessIdProperty();

    /**
     * @brief Gets the path of the executable that started the current process.
     *
     * C++ counterpart of .NET Environment.ProcessPath.
     * Returns an empty string when the path cannot be determined.
     */
    [[nodiscard]] static std::string getProcessPathProperty();

    // -------------------------------------------------------------------------
    // Environment variables
    // -------------------------------------------------------------------------

    /**
     * @brief Gets the value of an environment variable of the current process.
     *
     * C++ counterpart of .NET Environment.GetEnvironmentVariable(string).
     * @param name The name of the environment variable.
     * @return The value of the variable, or an empty string if not found.
     */
    [[nodiscard]] static std::string GetEnvironmentVariable(const std::string& name) {
        const char* val = std::getenv(name.c_str());
        return val ? std::string(val) : std::string();
    }

    /**
     * @brief Gets the value of an environment variable from the specified target.
     *
     * C++ counterpart of .NET Environment.GetEnvironmentVariable(string, EnvironmentVariableTarget).
     * Only EnvironmentVariableTarget::Process is supported; other targets fall back to Process.
     */
    [[nodiscard]] static std::string GetEnvironmentVariable(
            const std::string& name, EnvironmentVariableTarget) {
        return GetEnvironmentVariable(name);
    }

    /**
     * @brief Returns all environment variables of the current process as a map.
     *
     * C++ counterpart of .NET Environment.GetEnvironmentVariables() which returns IDictionary.
     * @return A map from variable name to value.
     */
    [[nodiscard]] static std::map<std::string, std::string> GetEnvironmentVariables();

    /**
     * @brief Returns all environment variables from the specified target.
     *
     * Only EnvironmentVariableTarget::Process is supported; other targets fall back to Process.
     */
    [[nodiscard]] static std::map<std::string, std::string> GetEnvironmentVariables(
            EnvironmentVariableTarget) {
        return GetEnvironmentVariables();
    }

    /**
     * @brief Sets an environment variable for the current process.
     *
     * C++ counterpart of .NET Environment.SetEnvironmentVariable(string, string).
     * Pass an empty string for @p value to remove the variable.
     */
    static void SetEnvironmentVariable(const std::string& name, const std::string& value);

    /**
     * @brief Sets an environment variable for the specified target.
     *
     * C++ counterpart of .NET Environment.SetEnvironmentVariable(string, string, EnvironmentVariableTarget).
     * Only EnvironmentVariableTarget::Process is supported; other targets delegate to Process.
     */
    static void SetEnvironmentVariable(const std::string& name, const std::string& value,
                                       EnvironmentVariableTarget) {
        SetEnvironmentVariable(name, value);
    }

    /**
     * @brief Replaces each environment variable token (%VAR%) with its value.
     *
     * C++ counterpart of .NET Environment.ExpandEnvironmentVariables(string).
     */
    [[nodiscard]] static std::string ExpandEnvironmentVariables(const std::string& name);

    // -------------------------------------------------------------------------
    // Special folders
    // -------------------------------------------------------------------------

    /**
     * @brief Gets the path to the specified system special folder.
     *
     * C++ counterpart of .NET Environment.GetFolderPath(SpecialFolder).
     */
    [[nodiscard]] static std::string GetFolderPath(SpecialFolder folder);

    /**
     * @brief Gets the path to the specified system special folder with the given option.
     *
     * C++ counterpart of .NET Environment.GetFolderPath(SpecialFolder, SpecialFolderOption).
     * The option is accepted for API compatibility but ignored on POSIX.
     */
    [[nodiscard]] static std::string GetFolderPath(SpecialFolder folder, SpecialFolderOption) {
        return GetFolderPath(folder);
    }

    // -------------------------------------------------------------------------
    // Hardware & OS
    // -------------------------------------------------------------------------

    /**
     * @brief Gets the number of logical processors on the current machine.
     *
     * C++ counterpart of .NET Environment.ProcessorCount.
     */
    [[nodiscard]] static SharpRuntime::intcs getProcessorCountProperty();

    /**
     * @brief Gets the size of the operating-system memory page in bytes.
     *
     * C++ counterpart of .NET Environment.SystemPageSize.
     */
    [[nodiscard]] static SharpRuntime::intcs getSystemPageSizeProperty();

    /**
     * @brief Gets a value indicating whether the current process is 64-bit.
     *
     * C++ counterpart of .NET Environment.Is64BitProcess.
     */
    [[nodiscard]] static bool Is64BitProcess() { return sizeof(void*) == 8; }

    /**
     * @brief Gets a value indicating whether the operating system is 64-bit.
     *
     * C++ counterpart of .NET Environment.Is64BitOperatingSystem.
     */
    [[nodiscard]] static bool Is64BitOperatingSystem() { return Is64BitProcess(); }

    // -------------------------------------------------------------------------
    // Machine / user identity
    // -------------------------------------------------------------------------

    /**
     * @brief Gets the NetBIOS name of the local computer.
     *
     * C++ counterpart of .NET Environment.MachineName.
     */
    [[nodiscard]] static std::string getMachineNameProperty();

    /**
     * @brief Gets the user name of the person currently logged on to the operating system.
     *
     * C++ counterpart of .NET Environment.UserName.
     */
    [[nodiscard]] static std::string getUserNameProperty();

    /**
     * @brief Gets the network domain name associated with the current user.
     *
     * C++ counterpart of .NET Environment.UserDomainName.
     * Returns the hostname on POSIX systems.
     */
    [[nodiscard]] static std::string getUserDomainNameProperty();

    // -------------------------------------------------------------------------
    // Time
    // -------------------------------------------------------------------------

    /**
     * @brief Gets the number of milliseconds elapsed since system startup (32-bit, wraps).
     *
     * C++ counterpart of .NET Environment.TickCount.
     */
    [[nodiscard]] static SharpRuntime::intcs getTickCountProperty() {
        return static_cast<SharpRuntime::intcs>(getTickCount64Property());
    }

    /**
     * @brief Gets the number of milliseconds elapsed since system startup (64-bit).
     *
     * C++ counterpart of .NET Environment.TickCount64.
     */
    [[nodiscard]] static SharpRuntime::longcs getTickCount64Property();

    // -------------------------------------------------------------------------
    // Memory
    // -------------------------------------------------------------------------

    /**
     * @brief Gets the amount of physical memory mapped to the process context.
     *
     * C++ counterpart of .NET Environment.WorkingSet.
     * Returns the resident set size in bytes on Linux, 0 on unsupported platforms.
     */
    [[nodiscard]] static SharpRuntime::longcs getWorkingSetProperty();

    // -------------------------------------------------------------------------
    // CPU usage
    // -------------------------------------------------------------------------

    /**
     * @brief Gets CPU usage statistics for the current process.
     *
     * C++ counterpart of .NET Environment.CpuUsage.
     */
    [[nodiscard]] static ProcessCpuUsage getCpuUsageProperty();

    // -------------------------------------------------------------------------
    // Command line
    // -------------------------------------------------------------------------

    /**
     * @brief Stores the process command-line arguments for later retrieval.
     *
     * Call this from main(argc, argv) so that GetCommandLineArgs() and
     * getCommandLineProperty() return meaningful values.
     */
    static void InitializeCommandLine(int argc, char** argv);

    /**
     * @brief Gets the command-line arguments for the current process.
     *
     * C++ counterpart of .NET Environment.GetCommandLineArgs().
     * Returns an empty vector unless InitializeCommandLine() was called first.
     */
    [[nodiscard]] static std::vector<std::string> GetCommandLineArgs();

    /**
     * @brief Gets the command line for the current process as a single string.
     *
     * C++ counterpart of .NET Environment.CommandLine.
     * Returns an empty string unless InitializeCommandLine() was called first.
     */
    [[nodiscard]] static std::string getCommandLineProperty();

    // -------------------------------------------------------------------------
    // Diagnostics (stubs)
    // -------------------------------------------------------------------------

    /**
     * @brief Gets the current stack trace information.
     *
     * C++ counterpart of .NET Environment.StackTrace.
     * Always returns an empty string in this implementation.
     */
    [[nodiscard]] static std::string getStackTraceProperty() { return ""; }

    // -------------------------------------------------------------------------
    // Process control
    // -------------------------------------------------------------------------

    /**
     * @brief Terminates the current process with the specified exit code.
     *
     * C++ counterpart of .NET Environment.Exit(int).
     */
    static void Exit(SharpRuntime::intcs exitCode) { std::exit(exitCode); }

    /**
     * @brief Terminates the process immediately without running finalizers.
     *
     * C++ counterpart of .NET Environment.FailFast(string).
     */
    static void FailFast(const std::string&) { std::abort(); }
};

} // namespace System
