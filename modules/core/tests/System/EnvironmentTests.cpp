// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "System/ArgumentException.hpp"
#include "System/Environment.hpp"
#include "System/IO/DirectoryNotFoundException.hpp"
#include "System/Version.hpp"
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using System::Environment;
using SharpRuntime::longcs;
using SharpRuntime::intcs;

// ---------------------------------------------------------------------------
// NewLine
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, NewLine_IsNonEmpty) {
    EXPECT_FALSE(Environment::NewLine.empty());
}

// ---------------------------------------------------------------------------
// GetCurrentDirectory
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, GetCurrentDirectory_ReturnsNonEmptyString) {
    EXPECT_FALSE(Environment::GetCurrentDirectory().empty());
}

// ---------------------------------------------------------------------------
// GetEnvironmentVariable
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, GetEnvironmentVariable_KnownVar_DoesNotThrow) {
    // PATH is present on Linux/macOS; just verify no exception
    std::string val = Environment::GetEnvironmentVariable("PATH");
    (void)val;
}

TEST(EnvironmentTests, GetEnvironmentVariable_NonExistent_ReturnsEmpty) {
    std::string val = Environment::GetEnvironmentVariable("SHARP_RUNTIME_NONEXISTENT_VAR_XYZ_12345");
    EXPECT_TRUE(val.empty());
}

// getenv("") is unspecified by POSIX; this must not crash and must behave like "not found",
// matching real .NET's GetEnvironmentVariable("") (returns null, no exception).
TEST(EnvironmentTests, GetEnvironmentVariable_EmptyName_ReturnsEmpty) {
    EXPECT_TRUE(Environment::GetEnvironmentVariable("").empty());
}

// ---------------------------------------------------------------------------
// ProcessorCount
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, ProcessorCount_AtLeastOne) {
    EXPECT_GE(Environment::getProcessorCountProperty(), 1);
}

// ---------------------------------------------------------------------------
// Is64Bit helpers
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, Is64BitProcess_ReturnsBool) {
    bool v = Environment::Is64BitProcess;
    (void)v;
}

TEST(EnvironmentTests, Is64BitOperatingSystem_ReturnsBool) {
    bool v = Environment::Is64BitOperatingSystem;
    (void)v;
}

TEST(EnvironmentTests, Is64BitOS_EqualsIs64BitProcess) {
    // This implementation delegates OS check to process check
    EXPECT_EQ(Environment::Is64BitOperatingSystem, Environment::Is64BitProcess);
}

// ---------------------------------------------------------------------------
// MachineName / UserName / TickCount64
// ---------------------------------------------------------------------------
TEST(EnvironmentTests, MachineName_NotEmpty) {
    std::string name = Environment::getMachineNameProperty();
    EXPECT_FALSE(name.empty());
}

#ifndef _WIN32
TEST(EnvironmentTests, MachineName_StripsDomainSuffix) {
    // Real .NET's Unix MachineName truncates at the first '.' to strip the domain suffix
    // (Environment.Unix.cs: hostName.Substring(0, hostName.IndexOf('.'))). Verify the
    // property's return value is never longer than the portion of the raw hostname before
    // its first '.', regardless of whether this particular host's name happens to be
    // fully-qualified.
    char raw[256]{};
    ASSERT_EQ(gethostname(raw, sizeof(raw)), 0);
    std::string rawHost(raw);
    auto dotPos = rawHost.find('.');
    std::string expected = dotPos == std::string::npos ? rawHost : rawHost.substr(0, dotPos);
    EXPECT_EQ(Environment::getMachineNameProperty(), expected);
}
#endif

TEST(EnvironmentTests, UserName_NotEmpty) {
    std::string name = Environment::getUserNameProperty();
    EXPECT_FALSE(name.empty());
}

TEST(EnvironmentTests, TickCount64_Positive) {
    SharpRuntime::longcs t = Environment::getTickCount64Property();
    EXPECT_GT(t, 0LL);
}

TEST(EnvironmentTests, TickCount64_Advances) {
    SharpRuntime::longcs t1 = Environment::getTickCount64Property();
    // int would signed-overflow well before 10,000,000 (true sum is ~5*10^13); volatile long long
    // burns the same CPU time without UB (found via UBSan, ticket 1486).
    volatile long long sink = 0;
    for (int i = 0; i < 10000000; ++i) sink += i;
    SharpRuntime::longcs t2 = Environment::getTickCount64Property();
    (void)sink;
    EXPECT_GE(t2, t1);
}

// ---------------------------------------------------------------------------
// HasShutdownStarted
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, HasShutdownStarted_IsFalse) {
    EXPECT_FALSE(Environment::HasShutdownStarted);
}

// ---------------------------------------------------------------------------
// SetEnvironmentVariable / GetEnvironmentVariable roundtrip
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, SetGet_RoundTrip) {
    Environment::SetEnvironmentVariable("SHARP_TEST_VAR", "hello");
    EXPECT_EQ(Environment::GetEnvironmentVariable("SHARP_TEST_VAR"), "hello");
}

TEST(EnvironmentTests, Set_Empty_RemovesVar) {
    Environment::SetEnvironmentVariable("SHARP_TEST_VAR2", "value");
    Environment::SetEnvironmentVariable("SHARP_TEST_VAR2", "");
    EXPECT_TRUE(Environment::GetEnvironmentVariable("SHARP_TEST_VAR2").empty());
}

// ---------------------------------------------------------------------------
// ExpandEnvironmentVariables
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, ExpandEnvVars_KnownVar) {
    Environment::SetEnvironmentVariable("SHARP_EXPAND_TEST", "world");
    std::string r = Environment::ExpandEnvironmentVariables("hello %SHARP_EXPAND_TEST%");
    EXPECT_EQ(r, "hello world");
}

TEST(EnvironmentTests, ExpandEnvVars_UnknownVar_Preserved) {
    std::string r = Environment::ExpandEnvironmentVariables("%SHARP_NONEXISTENT_XYZ%");
    EXPECT_EQ(r, "%SHARP_NONEXISTENT_XYZ%");
}

TEST(EnvironmentTests, ExpandEnvVars_NoVars_Unchanged) {
    EXPECT_EQ(Environment::ExpandEnvironmentVariables("no vars here"), "no vars here");
}

TEST(EnvironmentTests, ExpandEnvVars_EmptyString_ReturnsEmpty) {
    EXPECT_EQ(Environment::ExpandEnvironmentVariables(""), "");
}

TEST(EnvironmentTests, ExpandEnvVars_MultipleVars) {
    Environment::SetEnvironmentVariable("SHARP_EXPAND_A", "foo");
    Environment::SetEnvironmentVariable("SHARP_EXPAND_B", "bar");
    std::string r = Environment::ExpandEnvironmentVariables("%SHARP_EXPAND_A%-%SHARP_EXPAND_B%");
    EXPECT_EQ(r, "foo-bar");
}

// Real .NET's ExpandEnvironmentVariablesCore (Environment.UnixOrBrowser.cs) has a non-obvious
// property: when a %name% token fails to resolve, only the *opening* '%' is emitted as literal
// text -- the closing '%' is left to double as the opening delimiter of the next token, instead
// of both percents being consumed as one failed pair. Verified against the real algorithm by
// hand-tracing it; a naive "find the next '%' and consume both" scanner (this function's
// previous implementation) does not reproduce this.
TEST(EnvironmentTests, ExpandEnvVars_FailedTokenClosingPercentStartsNextToken) {
    Environment::SetEnvironmentVariable("SHARP_EXPAND_OVERLAP_HOME", "world");
    std::string r = Environment::ExpandEnvironmentVariables("%SHARP_EXPAND_OVERLAP_UNDEFINED%SHARP_EXPAND_OVERLAP_HOME%");
    EXPECT_EQ(r, "%SHARP_EXPAND_OVERLAP_UNDEFINEDworld");
}

TEST(EnvironmentTests, ExpandEnvVars_AdjacentPercents_Unchanged) {
    EXPECT_EQ(Environment::ExpandEnvironmentVariables("%%"), "%%");
}

// ---------------------------------------------------------------------------
// ProcessId
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, ProcessId_Positive) {
    EXPECT_GT(Environment::getProcessIdProperty(), 0);
}

// ---------------------------------------------------------------------------
// TickCount (int)
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, TickCount_Positive) {
    EXPECT_GT(Environment::getTickCountProperty(), 0);
}

// ---------------------------------------------------------------------------
// GetFolderPath
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, GetFolderPath_UserProfile_NonEmpty) {
    std::string p = Environment::GetFolderPath(Environment::SpecialFolder::UserProfile);
    EXPECT_FALSE(p.empty());
}

TEST(EnvironmentTests, GetFolderPath_Desktop_NonEmpty) {
    std::string p = Environment::GetFolderPath(Environment::SpecialFolder::Desktop);
    EXPECT_FALSE(p.empty());
}

// ---------------------------------------------------------------------------
// GetEnvironmentVariable with target
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, GetEnvironmentVariable_WithTarget_SameAsWithout) {
    std::string a = Environment::GetEnvironmentVariable("PATH");
    std::string b = Environment::GetEnvironmentVariable("PATH", System::EnvironmentVariableTarget::Process);
    EXPECT_EQ(a, b);
}

// ---------------------------------------------------------------------------
// IsPrivilegedProcess
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, IsPrivilegedProcess_ReturnsBool) {
    bool v = Environment::getIsPrivilegedProcessProperty();
    (void)v; // just verify it doesn't throw
}

// ---------------------------------------------------------------------------
// SetCurrentDirectory
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, SetCurrentDirectory_ThenGetReflectsChange) {
    std::string original = Environment::GetCurrentDirectory();
    // Change to /tmp and verify
    Environment::SetCurrentDirectory("/tmp");
    EXPECT_EQ(Environment::GetCurrentDirectory(), "/tmp");
    // Restore
    Environment::SetCurrentDirectory(original);
}

TEST(EnvironmentTests, SetCurrentDirectory_NonexistentPath_Throws) {
    // Regression: chdir()'s return value was previously ignored entirely, silently leaving
    // the working directory unchanged instead of surfacing the failure like real .NET does.
    EXPECT_THROW(Environment::SetCurrentDirectory("/this/path/does/not/exist/hopefully"),
                 System::IO::DirectoryNotFoundException);
}

TEST(EnvironmentTests, SetCurrentDirectory_EmptyPath_Throws) {
    // Regression: real .NET's setter calls ArgumentException.ThrowIfNullOrEmpty(value) before
    // touching the OS; this port previously had no such check.
    EXPECT_THROW(Environment::SetCurrentDirectory(""), System::ArgumentException);
}

// ---------------------------------------------------------------------------
// GetEnvironmentVariables
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, GetEnvironmentVariables_ContainsPATH) {
    auto vars = Environment::GetEnvironmentVariables();
    EXPECT_TRUE(vars.count("PATH") > 0);
}

TEST(EnvironmentTests, GetEnvironmentVariables_WithTarget_SameAsWithout) {
    auto a = Environment::GetEnvironmentVariables();
    auto b = Environment::GetEnvironmentVariables(System::EnvironmentVariableTarget::Process);
    EXPECT_EQ(a.size(), b.size());
}

TEST(EnvironmentTests, GetEnvironmentVariables_SetVar_Appears) {
    Environment::SetEnvironmentVariable("SHARP_MAP_TEST", "42");
    auto vars = Environment::GetEnvironmentVariables();
    ASSERT_TRUE(vars.count("SHARP_MAP_TEST") > 0);
    EXPECT_EQ(vars.at("SHARP_MAP_TEST"), "42");
}

// ---------------------------------------------------------------------------
// SetEnvironmentVariable with target
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, SetEnvironmentVariable_WithTarget_SetsForProcess) {
    Environment::SetEnvironmentVariable("SHARP_TARGET_VAR", "ok",
                                        System::EnvironmentVariableTarget::Process);
    EXPECT_EQ(Environment::GetEnvironmentVariable("SHARP_TARGET_VAR"), "ok");
}

// ---------------------------------------------------------------------------
// ProcessPath
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, ProcessPath_NonEmpty) {
    std::string path = Environment::getProcessPathProperty();
    EXPECT_FALSE(path.empty());
}

// ---------------------------------------------------------------------------
// SystemPageSize
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, SystemPageSize_PowerOfTwo) {
    int ps = Environment::getSystemPageSizeProperty();
    EXPECT_GT(ps, 0);
    EXPECT_EQ(ps & (ps - 1), 0); // must be a power of two
}

// ---------------------------------------------------------------------------
// UserDomainName
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, UserDomainName_NonEmpty) {
    EXPECT_FALSE(Environment::getUserDomainNameProperty().empty());
}

// ---------------------------------------------------------------------------
// WorkingSet
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, WorkingSet_NonNegative) {
    EXPECT_GE(Environment::getWorkingSetProperty(), 0LL);
}

// ---------------------------------------------------------------------------
// Command-line args
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, InitializeCommandLine_GetCommandLineArgs_Roundtrip) {
    const char* args[] = { "prog", "--flag", "value" };
    Environment::InitializeCommandLine(3, const_cast<char**>(args));
    auto v = Environment::GetCommandLineArgs();
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], "prog");
    EXPECT_EQ(v[1], "--flag");
    EXPECT_EQ(v[2], "value");
}

TEST(EnvironmentTests, CommandLine_JoinsArgs) {
    const char* args[] = { "prog", "arg1" };
    Environment::InitializeCommandLine(2, const_cast<char**>(args));
    EXPECT_EQ(Environment::getCommandLineProperty(), "prog arg1");
}

TEST(EnvironmentTests, CommandLine_EmptyWhenNotInitialized) {
    Environment::InitializeCommandLine(0, nullptr);
    EXPECT_EQ(Environment::getCommandLineProperty(), "");
}

// ---------------------------------------------------------------------------
// StackTrace (stub)
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, StackTrace_DoesNotThrow) {
    std::string s = Environment::getStackTraceProperty();
    (void)s;
}

// ---------------------------------------------------------------------------
// SpecialFolderOption — enum values
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, SpecialFolderOption_None_IsZero) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolderOption::None), 0);
}

TEST(EnvironmentTests, SpecialFolderOption_DoNotVerify_Is0x4000) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolderOption::DoNotVerify), 0x4000);
}

TEST(EnvironmentTests, SpecialFolderOption_Create_Is0x8000) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolderOption::Create), 0x8000);
}

// ---------------------------------------------------------------------------
// GetFolderPath with SpecialFolderOption
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, GetFolderPath_WithNone_SameAsWithout) {
    auto without = Environment::GetFolderPath(Environment::SpecialFolder::UserProfile);
    auto with    = Environment::GetFolderPath(Environment::SpecialFolder::UserProfile,
                                               Environment::SpecialFolderOption::None);
    EXPECT_EQ(without, with);
}

TEST(EnvironmentTests, GetFolderPath_WithDoNotVerify_SameAsWithout) {
    auto without = Environment::GetFolderPath(Environment::SpecialFolder::Desktop);
    auto with    = Environment::GetFolderPath(Environment::SpecialFolder::Desktop,
                                               Environment::SpecialFolderOption::DoNotVerify);
    EXPECT_EQ(without, with);
}

TEST(EnvironmentTests, GetFolderPath_WithCreate_SameAsWithout) {
    auto without = Environment::GetFolderPath(Environment::SpecialFolder::UserProfile);
    auto with    = Environment::GetFolderPath(Environment::SpecialFolder::UserProfile,
                                               Environment::SpecialFolderOption::Create);
    EXPECT_EQ(without, with);
}

// ---------------------------------------------------------------------------
// ProcessCpuUsage
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, CpuUsage_UserTime_NonNegative) {
    auto usage = Environment::getCpuUsageProperty();
    EXPECT_GE(usage.UserTime.getTotalSecondsProperty(), 0.0);
}

TEST(EnvironmentTests, CpuUsage_PrivilegedTime_NonNegative) {
    auto usage = Environment::getCpuUsageProperty();
    EXPECT_GE(usage.PrivilegedTime.getTotalSecondsProperty(), 0.0);
}

TEST(EnvironmentTests, CpuUsage_TotalTime_EqualsSumOfParts) {
    auto usage = Environment::getCpuUsageProperty();
    System::TimeSpan expected = usage.UserTime + usage.PrivilegedTime;
    EXPECT_EQ(usage.getTotalTimeProperty().getTicksProperty(), expected.getTicksProperty());
}

// ---------------------------------------------------------------------------
// OSVersion
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, OSVersion_PlatformIsValid) {
    auto osv = Environment::getOSVersionProperty();
    // Must be one of the known PlatformID values — not an arbitrary integer
    auto pid = osv.getPlatformProperty();
    bool valid = pid == System::PlatformID::Win32NT
              || pid == System::PlatformID::Unix
              || pid == System::PlatformID::MacOSX
              || pid == System::PlatformID::Other;
    EXPECT_TRUE(valid);
}

TEST(EnvironmentTests, OSVersion_VersionNonNegative) {
    auto osv = Environment::getOSVersionProperty();
    EXPECT_GE(osv.getVersionProperty().Major, 0);
    EXPECT_GE(osv.getVersionProperty().Minor, 0);
}

#ifdef __linux__
TEST(EnvironmentTests, OSVersion_LinuxPlatformIsUnix) {
    auto osv = Environment::getOSVersionProperty();
    EXPECT_EQ(osv.getPlatformProperty(), System::PlatformID::Unix);
}

TEST(EnvironmentTests, OSVersion_LinuxMajorVersionReasonable) {
    auto osv = Environment::getOSVersionProperty();
    // Linux kernel major version has been >= 2 since 1996
    EXPECT_GE(osv.getVersionProperty().Major, 2);
}
#endif

// ---------------------------------------------------------------------------
// UserInteractive
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, UserInteractive_IsTrue) {
    EXPECT_TRUE(Environment::getUserInteractiveProperty());
}

// ---------------------------------------------------------------------------
// SpecialFolder — enum numeric values (match CSIDL constants from .NET)
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, SpecialFolder_Desktop_Is0x0000) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::Desktop), 0x0000);
}
TEST(EnvironmentTests, SpecialFolder_Programs_Is0x0002) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::Programs), 0x0002);
}
TEST(EnvironmentTests, SpecialFolder_Personal_Is0x0005) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::Personal), 0x0005);
}
TEST(EnvironmentTests, SpecialFolder_MyDocuments_SameAsPersonal) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::MyDocuments),
              static_cast<int>(Environment::SpecialFolder::Personal));
}
TEST(EnvironmentTests, SpecialFolder_ApplicationData_Is0x001A) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::ApplicationData), 0x001A);
}
TEST(EnvironmentTests, SpecialFolder_LocalApplicationData_Is0x001C) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::LocalApplicationData), 0x001C);
}
TEST(EnvironmentTests, SpecialFolder_CommonApplicationData_Is0x0023) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::CommonApplicationData), 0x0023);
}
TEST(EnvironmentTests, SpecialFolder_UserProfile_Is0x0028) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::UserProfile), 0x0028);
}
TEST(EnvironmentTests, SpecialFolder_ProgramFiles_Is0x0026) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::ProgramFiles), 0x0026);
}
TEST(EnvironmentTests, SpecialFolder_System_Is0x0025) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::System), 0x0025);
}
TEST(EnvironmentTests, SpecialFolder_Windows_Is0x0024) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::Windows), 0x0024);
}
TEST(EnvironmentTests, SpecialFolder_MyPictures_Is0x0027) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::MyPictures), 0x0027);
}
TEST(EnvironmentTests, SpecialFolder_MyMusic_Is0x000D) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::MyMusic), 0x000D);
}
TEST(EnvironmentTests, SpecialFolder_MyVideos_Is0x000E) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::MyVideos), 0x000E);
}
TEST(EnvironmentTests, SpecialFolder_Fonts_Is0x0014) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::Fonts), 0x0014);
}
TEST(EnvironmentTests, SpecialFolder_Favorites_Is0x0006) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::Favorites), 0x0006);
}
TEST(EnvironmentTests, SpecialFolder_StartMenu_Is0x000B) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::StartMenu), 0x000B);
}
TEST(EnvironmentTests, SpecialFolder_CDBurning_Is0x003B) {
    EXPECT_EQ(static_cast<int>(Environment::SpecialFolder::CDBurning), 0x003B);
}

// ---------------------------------------------------------------------------
// GetFolderPath — additional folders
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, GetFolderPath_ApplicationData_NonEmpty) {
    std::string p = Environment::GetFolderPath(Environment::SpecialFolder::ApplicationData);
    EXPECT_FALSE(p.empty());
}
TEST(EnvironmentTests, GetFolderPath_LocalApplicationData_NonEmpty) {
    std::string p = Environment::GetFolderPath(Environment::SpecialFolder::LocalApplicationData);
    EXPECT_FALSE(p.empty());
}
TEST(EnvironmentTests, GetFolderPath_MyDocuments_NonEmpty) {
    std::string p = Environment::GetFolderPath(Environment::SpecialFolder::MyDocuments);
    EXPECT_FALSE(p.empty());
}
TEST(EnvironmentTests, GetFolderPath_Temp_IsString) {
    std::string p = Environment::GetFolderPath(Environment::SpecialFolder::ApplicationData);
    EXPECT_NE(p, "");
}

// ---------------------------------------------------------------------------
// CurrentManagedThreadId
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, CurrentManagedThreadId_NonNegative) {
    SharpRuntime::intcs id = Environment::getCurrentManagedThreadIdProperty();
    EXPECT_GE(id, 0);
}

TEST(EnvironmentTests, CurrentManagedThreadId_Idempotent) {
    SharpRuntime::intcs a = Environment::getCurrentManagedThreadIdProperty();
    SharpRuntime::intcs b = Environment::getCurrentManagedThreadIdProperty();
    EXPECT_EQ(a, b);
}

// ---------------------------------------------------------------------------
// Version
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, Version_MajorAtLeastOne) {
    System::Version v = Environment::getVersionProperty();
    EXPECT_GE(v.Major, 1);
}

TEST(EnvironmentTests, Version_MinorNonNegative) {
    System::Version v = Environment::getVersionProperty();
    EXPECT_GE(v.Minor, 0);
}

// ---------------------------------------------------------------------------
// EnvironmentVariableTarget — User/Machine are no-ops (matches .NET on non-Windows)
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, GetEnvironmentVariable_UserTarget_ReturnsEmpty) {
    Environment::SetEnvironmentVariable("SHARP_TARGET_NOOP_VAR", "process-value");
    EXPECT_TRUE(Environment::GetEnvironmentVariable(
        "SHARP_TARGET_NOOP_VAR", System::EnvironmentVariableTarget::User).empty());
}

TEST(EnvironmentTests, GetEnvironmentVariable_MachineTarget_ReturnsEmpty) {
    Environment::SetEnvironmentVariable("SHARP_TARGET_NOOP_VAR2", "process-value");
    EXPECT_TRUE(Environment::GetEnvironmentVariable(
        "SHARP_TARGET_NOOP_VAR2", System::EnvironmentVariableTarget::Machine).empty());
}

TEST(EnvironmentTests, SetEnvironmentVariable_UserTarget_DoesNotAffectProcess) {
    Environment::SetEnvironmentVariable("SHARP_TARGET_SET_NOOP", "unset", System::EnvironmentVariableTarget::Process);
    Environment::SetEnvironmentVariable("SHARP_TARGET_SET_NOOP", "user-value",
                                        System::EnvironmentVariableTarget::User);
    EXPECT_EQ(Environment::GetEnvironmentVariable("SHARP_TARGET_SET_NOOP"), "unset");
}

TEST(EnvironmentTests, GetEnvironmentVariables_UserTarget_IsEmpty) {
    auto vars = Environment::GetEnvironmentVariables(System::EnvironmentVariableTarget::User);
    EXPECT_TRUE(vars.empty());
}

TEST(EnvironmentTests, GetEnvironmentVariables_MachineTarget_IsEmpty) {
    auto vars = Environment::GetEnvironmentVariables(System::EnvironmentVariableTarget::Machine);
    EXPECT_TRUE(vars.empty());
}

// ---------------------------------------------------------------------------
// SetEnvironmentVariable — name validation (matches .NET Environment.ValidateVariable)
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, SetEnvironmentVariable_EmptyName_Throws) {
    EXPECT_THROW(Environment::SetEnvironmentVariable("", "value"), System::ArgumentException);
}

TEST(EnvironmentTests, SetEnvironmentVariable_NameContainsEquals_Throws) {
    EXPECT_THROW(Environment::SetEnvironmentVariable("BAD=NAME", "value"), System::ArgumentException);
}

TEST(EnvironmentTests, SetEnvironmentVariable_NameStartsWithNull_Throws) {
    std::string name("\0abc", 4);
    EXPECT_THROW(Environment::SetEnvironmentVariable(name, "value"), System::ArgumentException);
}

TEST(EnvironmentTests, SetEnvironmentVariable_WithUserTarget_EmptyName_Throws) {
    EXPECT_THROW(Environment::SetEnvironmentVariable("", "value", System::EnvironmentVariableTarget::User),
                 System::ArgumentException);
}

// ---------------------------------------------------------------------------
// ExitCode
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, ExitCode_DefaultsToZero) {
    EXPECT_EQ(Environment::getExitCodeProperty(), 0);
}

TEST(EnvironmentTests, ExitCode_SetGet_RoundTrip) {
    Environment::setExitCodeProperty(42);
    EXPECT_EQ(Environment::getExitCodeProperty(), 42);
    Environment::setExitCodeProperty(0); // restore default for other tests
}

// ---------------------------------------------------------------------------
// GetLogicalDrives
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, GetLogicalDrives_NonEmpty) {
    auto drives = Environment::GetLogicalDrives();
    EXPECT_FALSE(drives.empty());
}

#ifndef _WIN32
TEST(EnvironmentTests, GetLogicalDrives_ContainsRoot) {
    auto drives = Environment::GetLogicalDrives();
    EXPECT_NE(std::find(drives.begin(), drives.end(), "/"), drives.end());
}
#endif

// ---------------------------------------------------------------------------
// SystemDirectory
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, SystemDirectory_MatchesGetFolderPathSystem) {
    EXPECT_EQ(Environment::getSystemDirectoryProperty(),
              Environment::GetFolderPath(Environment::SpecialFolder::System));
}

// ---------------------------------------------------------------------------
// GetCurrentDirectory has no length ceiling (#2240 / SR-AUD-107)
//
// GetCurrentDirectory() used to call getcwd() once into a fixed char[4096] and return ""
// on ERANGE, so a legal current directory past 4 KiB -- reachable by chdir()ing one
// component at a time -- became an empty string. Measured before the fix
// (build-probe/2239_probe1_before.log): a real 4,868-byte cwd returned length 0.
// See docs/CoreEnvironmentCompatibleSlicePlan.md section 4.1.
// ---------------------------------------------------------------------------

#ifndef _WIN32
namespace {

// getcwd() with a buffer that grows until the call succeeds -- the independent reference
// this test compares the public port against.
std::string dynamicGetcwd() {
    std::vector<char> buf(256);
    for (;;) {
        if (::getcwd(buf.data(), buf.size())) return std::string(buf.data());
        if (errno != ERANGE) return "";
        buf.resize(buf.size() * 2);
    }
}

} // namespace

TEST(EnvironmentTests, GetCurrentDirectory_LongPath_IsNotTruncatedOrLost) {
    const std::string original = dynamicGetcwd();
    ASSERT_FALSE(original.empty());

    // A uniquely named root under the test's own working directory, so this never touches
    // a shared temp path and never collides with a concurrently running suite.
    const std::string root = "sr_2240_deep_" + std::to_string(::getpid());
    if (::mkdir(root.c_str(), 0777) != 0 || ::chdir(root.c_str()) != 0) {
        GTEST_SKIP() << "could not create a scratch directory in " << original;
    }

    // Every mkdir/chdir uses a RELATIVE 200-byte name, so no single pathname argument ever
    // approaches PATH_MAX -- only the resulting cwd grows.
    const std::string component(200, 'd');
    int depth = 0;
    while (dynamicGetcwd().size() < 4600) {
        if (::mkdir(component.c_str(), 0777) != 0 && errno != EEXIST) break;
        if (::chdir(component.c_str()) != 0) break;
        ++depth;
    }
    const std::string actual = dynamicGetcwd();
    const bool deepEnough = actual.size() > 4096;

    if (deepEnough) {
        EXPECT_EQ(Environment::GetCurrentDirectory(), actual);
        EXPECT_EQ(Environment::GetCurrentDirectory().size(), actual.size());
    }

    // Unwind bottom-up so no long pathname is ever passed to the OS, then restore.
    for (int i = 0; i < depth; ++i) {
        const std::string here = dynamicGetcwd();
        const std::string leaf = here.substr(here.rfind('/') + 1);
        if (::chdir("..") != 0) break;
        ::rmdir(leaf.c_str());
    }
    ASSERT_EQ(::chdir(original.c_str()), 0);
    ::rmdir(root.c_str());
    ASSERT_EQ(dynamicGetcwd(), original);

    if (!deepEnough) {
        GTEST_SKIP() << "filesystem would not accept a current directory past 4 KiB (reached "
                     << actual.size() << " bytes)";
    }
}

TEST(EnvironmentTests, GetCurrentDirectory_ShortPath_MatchesDynamicGetcwd) {
    // The control the repair must not move: an ordinary cwd is reported byte-identically.
    EXPECT_EQ(Environment::GetCurrentDirectory(), dynamicGetcwd());
}

TEST(EnvironmentTests, ProcessPath_IsAbsoluteAndNotTruncated) {
    const std::string path = Environment::getProcessPathProperty();
    ASSERT_FALSE(path.empty());
    EXPECT_EQ(path.front(), '/');
    // readlink() reports truncation only by filling the buffer, so the repaired loop must
    // agree with an independently sized read of the same link.
    std::vector<char> buf(65536);
    const ssize_t len = ::readlink("/proc/self/exe", buf.data(), buf.size());
    if (len > 0 && static_cast<std::size_t>(len) < buf.size()) {
        EXPECT_EQ(path, std::string(buf.data(), static_cast<std::size_t>(len)));
    }
}
#endif // !_WIN32

// ---------------------------------------------------------------------------
// CommandLine quotes and escapes as PasteArguments does (#2241 / SR-AUD-108)
//
// getCommandLineProperty() used to space-join the raw argv entries, so an argument
// containing whitespace or a quote could not be told apart from two arguments. .NET builds
// this string with PasteArguments.Paste(..., pasteFirstArgumentUsingArgV0Rules: true).
// Each case below asserts the exact emitted text AND re-parses it with a
// CommandLineToArgvW-shaped reference parser, so the round trip is measured rather than
// asserted. See docs/CoreEnvironmentCompatibleSlicePlan.md sections 4.2 and 4.3.
// ---------------------------------------------------------------------------

namespace {

// The inverse of PasteArguments.AppendArgument, i.e. CommandLineToArgvW's rules for every
// argument after argv[0]. Deliberately written independently of the production code.
std::vector<std::string> parseCommandLineTail(const std::string& s) {
    auto isWs = [](char c) { return c == ' ' || c == '\t'; };
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && isWs(s[i])) ++i;
        if (i >= s.size()) break;
        std::string arg;
        bool inQuotes = false;
        while (i < s.size()) {
            if (s[i] == '\\') {
                std::size_t n = 0;
                while (i < s.size() && s[i] == '\\') { ++n; ++i; }
                if (i < s.size() && s[i] == '"') {
                    arg.append(n / 2, '\\');
                    if (n % 2) { arg.push_back('"'); ++i; }
                    else { inQuotes = !inQuotes; ++i; }
                } else {
                    arg.append(n, '\\');
                }
                continue;
            }
            if (s[i] == '"') { inQuotes = !inQuotes; ++i; continue; }
            if (!inQuotes && isWs(s[i])) break;
            arg.push_back(s[i++]);
        }
        out.push_back(arg);
    }
    return out;
}

std::string commandLineOf(const std::vector<std::string>& argv) {
    std::vector<char*> raw;
    raw.reserve(argv.size());
    for (const auto& a : argv) raw.push_back(const_cast<char*>(a.c_str()));
    Environment::InitializeCommandLine(static_cast<int>(raw.size()),
                                       raw.empty() ? nullptr : raw.data());
    return Environment::getCommandLineProperty();
}

} // namespace

TEST(EnvironmentTests, CommandLine_PlainArguments_AreEmittedVerbatim) {
    // The common case must stay byte-identical to the old space-join.
    EXPECT_EQ(commandLineOf({"prog", "arg1"}), "prog arg1");
    EXPECT_EQ(commandLineOf({"prog", "--flag", "value"}), "prog --flag value");
}

TEST(EnvironmentTests, CommandLine_WhitespaceAndQuotes_AreQuotedAndEscaped) {
    EXPECT_EQ(commandLineOf({"prog", "two words"}),    "prog \"two words\"");
    EXPECT_EQ(commandLineOf({"prog", "a\tb"}),         "prog \"a\tb\"");
    EXPECT_EQ(commandLineOf({"prog", "a\nb"}),         "prog \"a\nb\"");
    EXPECT_EQ(commandLineOf({"prog", "quote\"value"}), "prog \"quote\\\"value\"");
    EXPECT_EQ(commandLineOf({"prog", ""}),             "prog \"\"");
}

TEST(EnvironmentTests, CommandLine_BackslashRuns_FollowThe2nAnd2nPlus1Rules) {
    // Two backslashes then a quote -> 2*2+1 = five backslashes then an escaped quote.
    EXPECT_EQ(commandLineOf({"prog", "a\\\\\"b"}), "prog \"a\\\\\\\\\\\"b\"");
    // Trailing backslashes are doubled because a closing quote follows them.
    EXPECT_EQ(commandLineOf({"prog", "a b\\"}),    "prog \"a b\\\\\"");
    // A backslash not followed by a quote is NOT doubled.
    EXPECT_EQ(commandLineOf({"prog", "a\\b c"}),   "prog \"a\\b c\"");
}

TEST(EnvironmentTests, CommandLine_EveryCaseRoundTripsThroughAReferenceParser) {
    const std::vector<std::vector<std::string>> cases = {
        {"prog", "arg1"},
        {"prog", "two words"},
        {"prog", "a\tb"},
        {"prog", "quote\"value"},
        {"prog", ""},
        {"prog", "a\\\\\"b"},
        {"prog", "a b\\"},
        {"prog", "a\\b c"},
        {"prog", "one", "t w o", "thr\"ee", ""},
    };
    for (const auto& argv : cases) {
        const std::string text = commandLineOf(argv);
        std::vector<std::string> expected(argv.begin() + 1, argv.end());
        EXPECT_EQ(parseCommandLineTail(text.substr(text.find(' ') + 1)), expected)
            << "emitted: " << text;
    }
}

TEST(EnvironmentTests, CommandLine_ArgV0_UsesTheArgV0Rules) {
    // argv[0] parses under different rules: a backslash is an ordinary character and quotes
    // exist only to carry whitespace.
    EXPECT_EQ(commandLineOf({"my prog", "x"}), "\"my prog\" x");
    EXPECT_EQ(commandLineOf({"C:\\dir\\prog", "x"}), "C:\\dir\\prog x");
    EXPECT_EQ(commandLineOf({""}), "\"\"");
    // PINNED, and deferred to ticket #2242: argv[0]'s rules cannot represent a literal
    // quote, and what .NET does when asked to is not decidable in this container. This port
    // emits it verbatim rather than throwing from a public diagnostic property.
    EXPECT_EQ(commandLineOf({"pro\"g", "x"}), "pro\"g x");
}

TEST(EnvironmentTests, CommandLine_EmptyArgvIsStillEmpty) {
    EXPECT_EQ(commandLineOf({}), "");
}
