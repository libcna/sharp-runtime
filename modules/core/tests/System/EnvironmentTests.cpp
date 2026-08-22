// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <utility>
#include "System/detail/SpecialFolderDefinedness.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <optional>
#include <atomic>
#include <cstdint>
#include <random>
#include <stdexcept>
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
    const std::optional<std::string> val = Environment::GetEnvironmentVariable("PATH");
    EXPECT_TRUE(val.has_value()) << "PATH is expected to be present";
}

TEST(EnvironmentTests, GetEnvironmentVariable_NonExistent_ReturnsNullopt) {
    // #2313: nullopt, not "". The old assertion could not tell an absent variable from a present
    // empty one, which is the whole finding.
    EXPECT_EQ(std::nullopt,
              Environment::GetEnvironmentVariable("SHARP_RUNTIME_NONEXISTENT_VAR_XYZ_12345"));
}

// getenv("") is unspecified by POSIX; this must not crash and must behave like "not found",
// matching real .NET's GetEnvironmentVariable("") (returns null, no exception).
TEST(EnvironmentTests, GetEnvironmentVariable_EmptyName_ReturnsNullopt) {
    EXPECT_EQ(std::nullopt, Environment::GetEnvironmentVariable(""));
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

// #2313 RESOLVED, AND IT REVERSED BOTH TESTS BELOW.
//
// The old comment here recorded the defect precisely: "GetEnvironmentVariable alone cannot pin
// removal: it returns \"\" for an absent variable *and* for one that is present with an empty
// value". That is fixed -- the getter returns std::optional<std::string> -- and the SETTER's rule
// changed with it. `Environment.Variables.Unix.cs:49-57` removes ONLY on a null value and stores
// everything else, empty strings included, and there is no empty-to-null conversion on the read
// side either.
TEST(EnvironmentTests, Fix2313_AnEmptyValueIsStoredAndOnlyNulloptRemoves) {
    Environment::SetEnvironmentVariable("SHARP_TEST_VAR2", "value");
    ASSERT_EQ(Environment::GetEnvironmentVariables().count("SHARP_TEST_VAR2"), std::size_t(1));

    // THE BEHAVIOUR THAT REVERSED. This used to delete the variable.
    Environment::SetEnvironmentVariable("SHARP_TEST_VAR2", "");
    EXPECT_EQ(Environment::GetEnvironmentVariables().count("SHARP_TEST_VAR2"), std::size_t(1))
        << "an empty value must STORE, not remove";
    EXPECT_EQ(std::optional<std::string>(""), Environment::GetEnvironmentVariable("SHARP_TEST_VAR2"))
        << "present with an empty value";

    // ...and the removal that used to be spelled "" is now spelled nullopt.
    Environment::SetEnvironmentVariable("SHARP_TEST_VAR2", std::nullopt);
    EXPECT_EQ(Environment::GetEnvironmentVariables().count("SHARP_TEST_VAR2"), std::size_t(0));
    EXPECT_EQ(std::nullopt, Environment::GetEnvironmentVariable("SHARP_TEST_VAR2"));
}

TEST(EnvironmentTests, Fix2313_TheGetterTellsAbsentFromPresentEmpty) {
    // The single assertion the whole finding is about, and the one the old suite explicitly could
    // not make: these two states must now DIFFER.
    Environment::SetEnvironmentVariable("SHARP_PRESENT_EMPTY_2313", "");
    Environment::SetEnvironmentVariable("SHARP_ABSENT_2313", std::nullopt);

    EXPECT_EQ(std::optional<std::string>(""),
              Environment::GetEnvironmentVariable("SHARP_PRESENT_EMPTY_2313"));
    EXPECT_EQ(std::nullopt, Environment::GetEnvironmentVariable("SHARP_ABSENT_2313"));
    EXPECT_NE(Environment::GetEnvironmentVariable("SHARP_PRESENT_EMPTY_2313"),
              Environment::GetEnvironmentVariable("SHARP_ABSENT_2313"))
        << "these were indistinguishable before #2313";

    Environment::SetEnvironmentVariable("SHARP_PRESENT_EMPTY_2313", std::nullopt);
}

#ifndef _WIN32
// The out-of-band route is kept because it proves the PLATFORM's state, independently of this
// port's setter -- so if the setter ever regressed to deleting, this test would still hold and
// the one above would be the one to fail. POSIX-only: the Windows CRT's _putenv_s removes a
// variable set to "", which is why the setter now goes through SetEnvironmentVariableA there.
TEST(EnvironmentTests, Fix2313_APlatformInstalledEmptyValueIsAlsoDistinguished) {
    ASSERT_EQ(::setenv("SHARP_PRESENT_EMPTY", "", 1), 0);
    const auto vars = Environment::GetEnvironmentVariables();
    const auto it = vars.find("SHARP_PRESENT_EMPTY");
    ASSERT_NE(it, vars.end());
    EXPECT_TRUE(it->second.empty());
    EXPECT_EQ(vars.count("SHARP_ABSENT_NEVER_SET"), std::size_t(0));

    // This assertion is INVERTED. It used to be EXPECT_EQ, pinning that the getter could not tell
    // them apart.
    EXPECT_NE(Environment::GetEnvironmentVariable("SHARP_PRESENT_EMPTY"),
              Environment::GetEnvironmentVariable("SHARP_ABSENT_NEVER_SET"));
    EXPECT_EQ(std::optional<std::string>(""),
              Environment::GetEnvironmentVariable("SHARP_PRESENT_EMPTY"));
    ASSERT_EQ(::unsetenv("SHARP_PRESENT_EMPTY"), 0);
}
#endif // !_WIN32

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
    // DoNotVerify, because since #2320 the DEFAULT option verifies and returns "" for a
    // directory that does not exist. This row is about the PATH being mapped at all, and a
    // container without a ~/Desktop must not turn that into a failure (SA-6).
    std::string p = Environment::GetFolderPath(Environment::SpecialFolder::Desktop,
                                               Environment::SpecialFolderOption::DoNotVerify);
    EXPECT_FALSE(p.empty());
}

// ---------------------------------------------------------------------------
// GetEnvironmentVariable with target
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, GetEnvironmentVariable_WithTarget_SameAsWithout) {
    const auto a = Environment::GetEnvironmentVariable("PATH");
    const auto b = Environment::GetEnvironmentVariable("PATH", System::EnvironmentVariableTarget::Process);
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

TEST(EnvironmentTests, GetFolderPath_WithDoNotVerify_AgreesOnlyWhenTheDirectoryExists) {
    // REWRITTEN BY #2320, because it used to assert the opposite of the contract. `DoNotVerify`
    // and the default AGREE exactly when the directory is readable, and DIFFER otherwise -- that
    // difference is the whole point of the option, and the old row passed only because this
    // machine happened to have a ~/Desktop.
    const auto path     = Environment::GetFolderPath(Environment::SpecialFolder::Desktop,
                                                     Environment::SpecialFolderOption::DoNotVerify);
    const auto verified = Environment::GetFolderPath(Environment::SpecialFolder::Desktop);
    ASSERT_FALSE(path.empty());
    if (::access(path.c_str(), R_OK) == 0) {
        EXPECT_EQ(path, verified);
    } else {
        EXPECT_TRUE(verified.empty()) << "an unreadable directory must verify to \"\"";
    }
}

TEST(EnvironmentTests, GetFolderPath_WithCreate_AgreesWhenTheDirectoryAlreadyExists) {
    // The home directory exists wherever this suite can run at all, so Create has nothing to do
    // and must simply agree. The interesting half -- Create on a MISSING directory -- is
    // XdgAndFolderOptionTests below, which builds its own missing directory rather than hoping
    // for one.
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
    // DoNotVerify -- see GetFolderPath_Desktop_NonEmpty; the default verifies since #2320.
    std::string p = Environment::GetFolderPath(Environment::SpecialFolder::ApplicationData,
                                               Environment::SpecialFolderOption::DoNotVerify);
    EXPECT_FALSE(p.empty());
}
TEST(EnvironmentTests, GetFolderPath_LocalApplicationData_NonEmpty) {
    // DoNotVerify -- see GetFolderPath_Desktop_NonEmpty; the default verifies since #2320.
    std::string p = Environment::GetFolderPath(Environment::SpecialFolder::LocalApplicationData,
                                               Environment::SpecialFolderOption::DoNotVerify);
    EXPECT_FALSE(p.empty());
}
TEST(EnvironmentTests, GetFolderPath_MyDocuments_NonEmpty) {
    // DoNotVerify -- see GetFolderPath_Desktop_NonEmpty; the default verifies since #2320.
    std::string p = Environment::GetFolderPath(Environment::SpecialFolder::MyDocuments,
                                               Environment::SpecialFolderOption::DoNotVerify);
    EXPECT_FALSE(p.empty());
}
// ---------------------------------------------------------------------------
// #2364 -- the eight rows #2320 left, aligned 2026-08-19.
//
// Every case uses DoNotVerify, so what is asserted is the MAPPING rather than whether this
// machine happens to have the directory. That is the #2320 / SA-6 lesson stated as a rule: five of
// its rows passed only because this container has ~/Desktop, which SA-6 calls a defect in the
// TEST rather than evidence about the code.
//
// $HOME is set explicitly per case rather than read, for the same reason -- a path derived from
// the machine's own environment cannot discriminate a mapping from a coincidence.
// ---------------------------------------------------------------------------

namespace {
    /// Restores HOME and every XDG variable this suite perturbs, whatever the case does.
    class ScopedFolderEnv {
        std::vector<std::pair<std::string, std::optional<std::string>>> saved_;
    public:
        void set(const std::string& name, const std::optional<std::string>& value) {
            saved_.emplace_back(name, Environment::GetEnvironmentVariable(name));
            Environment::SetEnvironmentVariable(name, value);
        }
        ~ScopedFolderEnv() {
            for (auto it = saved_.rbegin(); it != saved_.rend(); ++it)
                Environment::SetEnvironmentVariable(it->first, it->second);
        }
    };

    std::string folder(Environment::SpecialFolder f) {
        return Environment::GetFolderPath(f, Environment::SpecialFolderOption::DoNotVerify);
    }

    /// Gives filesystem-backed Environment tests an isolated root and removes every descendant.
    class ScopedTemporaryDirectory final {
    public:
        ScopedTemporaryDirectory() {
            const auto parent = std::filesystem::temp_directory_path();
            static std::atomic<std::uint64_t> sequence{0};
            std::random_device entropy;
            for (int attempt = 0; attempt < 128; ++attempt) {
                const auto random =
                    (static_cast<std::uint64_t>(entropy()) << 32U) ^ entropy();
                const auto suffix =
                    random ^ sequence.fetch_add(1, std::memory_order_relaxed);
                auto candidate =
                    parent / ("sharp-runtime-environment-" + std::to_string(suffix));
                std::error_code error;
                if (std::filesystem::create_directory(candidate, error)) {
                    path_ = std::move(candidate);
                    return;
                }
            }
            throw std::runtime_error(
                "could not create a unique Environment-test directory");
        }

        ~ScopedTemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }

        ScopedTemporaryDirectory(const ScopedTemporaryDirectory&) = delete;
        ScopedTemporaryDirectory& operator=(const ScopedTemporaryDirectory&) = delete;

        [[nodiscard]] std::string path(const std::filesystem::path& relative) const {
            return (path_ / relative).string();
        }

    private:
        std::filesystem::path path_;
    };
}

TEST(EnvironmentTests, Fix2364_TheSixUserDirectoriesGoThroughReadXdgDirectory) {
    // GetFolderPathCore.Unix.cs:88-89, 119-121, 133-140. Each falls back to home/<name> when the
    // variable is unset and no user-dirs.dirs entry matches -- which is the shape this case pins,
    // with an XDG config directory pointed at somewhere that has no user-dirs.dirs at all.
    ScopedFolderEnv env;
    env.set("HOME", "/tmp-not-used/h2364");
    env.set("XDG_CONFIG_HOME", "/tmp-not-used/h2364/emptyconfig");
    for (const char* key : {"XDG_DOCUMENTS_DIR", "XDG_DESKTOP_DIR", "XDG_MUSIC_DIR",
                            "XDG_PICTURES_DIR", "XDG_VIDEOS_DIR", "XDG_TEMPLATES_DIR"})
        env.set(key, std::nullopt);

    // THE SHARPEST ROW: this port used to return the HOME DIRECTORY ITSELF for Personal, so an
    // application writing "the user's documents" wrote into $HOME.
    EXPECT_EQ(folder(Environment::SpecialFolder::Personal), "/tmp-not-used/h2364/Documents");
    EXPECT_EQ(folder(Environment::SpecialFolder::MyDocuments), "/tmp-not-used/h2364/Documents")
        << "MyDocuments == Personal (0x0005), so it must move with it";
    EXPECT_EQ(folder(Environment::SpecialFolder::Desktop), "/tmp-not-used/h2364/Desktop");
    EXPECT_EQ(folder(Environment::SpecialFolder::DesktopDirectory), "/tmp-not-used/h2364/Desktop");
    EXPECT_EQ(folder(Environment::SpecialFolder::MyMusic), "/tmp-not-used/h2364/Music");
    EXPECT_EQ(folder(Environment::SpecialFolder::MyPictures), "/tmp-not-used/h2364/Pictures");
    EXPECT_EQ(folder(Environment::SpecialFolder::MyVideos), "/tmp-not-used/h2364/Videos");
    EXPECT_EQ(folder(Environment::SpecialFolder::Templates), "/tmp-not-used/h2364/Templates");

    // UserProfile is home ITSELF and must NOT have moved with Personal -- the two used to be the
    // same answer, so a repair that moved both would look right here without this row.
    EXPECT_EQ(folder(Environment::SpecialFolder::UserProfile), "/tmp-not-used/h2364");
}

TEST(EnvironmentTests, Fix2364_AnAbsoluteXdgVariableWinsAndARelativeOneIsIgnored) {
    // ReadXdgDirectory:220-224 -- the variable is used only when it is ABSOLUTE. A relative value
    // is ignored, not joined, which is the same rule xdgBase already applied to the two bases.
    ScopedFolderEnv env;
    env.set("HOME", "/tmp-not-used/h2364");
    env.set("XDG_CONFIG_HOME", "/tmp-not-used/h2364/emptyconfig");

    env.set("XDG_MUSIC_DIR", "/absolute/tunes");
    EXPECT_EQ(folder(Environment::SpecialFolder::MyMusic), "/absolute/tunes");

    env.set("XDG_PICTURES_DIR", "relative/pics");
    EXPECT_EQ(folder(Environment::SpecialFolder::MyPictures), "/tmp-not-used/h2364/Pictures")
        << "a relative XDG value is IGNORED, not joined onto home";

    env.set("XDG_VIDEOS_DIR", "");
    EXPECT_EQ(folder(Environment::SpecialFolder::MyVideos), "/tmp-not-used/h2364/Videos")
        << "an empty value is not absolute either";
}

TEST(EnvironmentTests, Fix2364_TheUserDirsFileIsReadAndItsGrammarIsDotNets) {
    // ReadXdgDirectory:184-248. The file is read out of the XDG CONFIG directory -- so it honours
    // XDG_CONFIG_HOME rather than a hard-coded ~/.config -- and each rejection below is one .NET
    // makes rather than a simplification.
    namespace fs = std::filesystem;
    ScopedTemporaryDirectory temporary;
    const fs::path sandbox = temporary.path("sandbox-2364");
    fs::create_directories(sandbox / "config");

    {
        std::ofstream out(sandbox / "config" / "user-dirs.dirs");
        out << "# a comment line, skipped\n";
        out << "XDG_DESKTOP_DIR=\"$HOME/Bureau\"\n";              // $HOME-relative
        out << "   XDG_MUSIC_DIR   =   \"/srv/music\"\n";          // whitespace everywhere
        out << "XDG_PICTURES_DIR=\"notabsolute/pics\"\n";          // neither $HOME/ nor / -> SKIPPED
        out << "XDG_VIDEOS_DIR=\"\"\n";                            // empty value -> SKIPPED
    }

    ScopedFolderEnv env;
    env.set("HOME", (sandbox / "home").string());
    env.set("XDG_CONFIG_HOME", (sandbox / "config").string());
    for (const char* key : {"XDG_DESKTOP_DIR", "XDG_MUSIC_DIR", "XDG_PICTURES_DIR",
                            "XDG_VIDEOS_DIR"})
        env.set(key, std::nullopt);

    EXPECT_EQ(folder(Environment::SpecialFolder::Desktop),
              (sandbox / "home" / "Bureau").string())
        << "the $HOME/ prefix is stripped and the rest joined onto home";
    EXPECT_EQ(folder(Environment::SpecialFolder::MyMusic), "/srv/music")
        << "whitespace before the key, around '=' and before the quote is skipped";
    EXPECT_EQ(folder(Environment::SpecialFolder::MyPictures),
              (sandbox / "home" / "Pictures").string())
        << "a value that is neither $HOME/-relative nor absolute is SKIPPED, not accepted";
    EXPECT_EQ(folder(Environment::SpecialFolder::MyVideos),
              (sandbox / "home" / "Videos").string())
        << "an empty value is skipped";

}

TEST(EnvironmentTests, Fix2364_TheTwoStaticPathsAndTheTwoDeliberatelyUnmappedOnes) {
    // GetFolderPathCore.Unix.cs:52-60.
    ScopedFolderEnv env;
    env.set("HOME", "/tmp-not-used/h2364");

    // Static, resolved before .NET even looks up the home directory.
    EXPECT_EQ(folder(Environment::SpecialFolder::CommonApplicationData), "/usr/share")
        << "#2364: was \"/etc\", which is not writable by a non-root user and is not where shared "
           "application data belongs";
    EXPECT_EQ(folder(Environment::SpecialFolder::CommonTemplates), "/usr/share/templates")
        << "#2364: was unmapped";

    // Fonts moved from a SYSTEM path to a USER one.
    EXPECT_EQ(folder(Environment::SpecialFolder::Fonts), "/tmp-not-used/h2364/.fonts")
        << "#2364: was \"/usr/share/fonts\"";

    // THE REVERSE SHAPE, and the reason it is worth its own assertions: .NET maps these two only
    // under TARGET_OSX, so on Linux no arm matches and the answer is "". This port INVENTED
    // "/usr" and "/usr/lib", so here the alignment REMOVES a mapping rather than adding one.
    EXPECT_EQ(folder(Environment::SpecialFolder::ProgramFiles), "")
        << "#2364: was \"/usr\" -- an invented mapping where .NET has none on Linux";
    EXPECT_EQ(folder(Environment::SpecialFolder::System), "")
        << "#2364: was \"/usr/lib\" -- likewise";
}

TEST(EnvironmentTests, Decl2364_AnUnknownHomeStillFallsBackToRoot) {
    // GetFolderPathCore.Unix.cs:76-81, unchanged by #2364 and asserted because the new rows all
    // build on `home`: "/" is not writable by a non-root user, which .NET states as a SAFEGUARD
    // rather than a tidy default. A row that silently produced "Documents" with no leading slash
    // would be a place an application could write private data.
    ScopedFolderEnv env;
    env.set("HOME", std::nullopt);
    env.set("XDG_CONFIG_HOME", std::nullopt);
    env.set("XDG_DOCUMENTS_DIR", std::nullopt);

    EXPECT_EQ(folder(Environment::SpecialFolder::UserProfile), "/");
    const std::string documents = folder(Environment::SpecialFolder::Personal);
    EXPECT_FALSE(documents.empty());
    EXPECT_EQ(documents.front(), '/') << "every derived row must stay absolute";
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

// Environment::Version is Version(1, 0, 0) -- a three-component version, and the
// only partially specified Version any first-party API hands out. Since #2258
// (SR-AUD-011) asking it for a fourth field is rejected rather than answered
// with the "-1" sentinel, so this pins the one place the narrowed argument
// domain is reachable from a public property rather than from a literal.
TEST(EnvironmentTests, Version_RevisionIsUndefinedAndCannotBeRequested) {
    System::Version v = Environment::getVersionProperty();
    ASSERT_LT(v.Revision, 0);
    EXPECT_EQ(v.ToString(), "1.0.0");
    EXPECT_EQ(v.ToString(3), "1.0.0");
    EXPECT_THROW(v.ToString(4), System::ArgumentException);
}

// ---------------------------------------------------------------------------
// EnvironmentVariableTarget — User/Machine are no-ops (matches .NET on non-Windows)
// ---------------------------------------------------------------------------

TEST(EnvironmentTests, GetEnvironmentVariable_UserTarget_ReturnsEmpty) {
    Environment::SetEnvironmentVariable("SHARP_TARGET_NOOP_VAR", "process-value");
    // #2313: nullopt, not "". A non-Process target has no store here, so nothing is PRESENT in
    // it -- reporting "" would claim the variable exists and is empty.
    EXPECT_EQ(std::nullopt, Environment::GetEnvironmentVariable(
        "SHARP_TARGET_NOOP_VAR", System::EnvironmentVariableTarget::User));
}

TEST(EnvironmentTests, GetEnvironmentVariable_MachineTarget_ReturnsEmpty) {
    Environment::SetEnvironmentVariable("SHARP_TARGET_NOOP_VAR2", "process-value");
    // #2313: nullopt, not "". A non-Process target has no store here, so nothing is PRESENT in
    // it -- reporting "" would claim the variable exists and is empty.
    EXPECT_EQ(std::nullopt, Environment::GetEnvironmentVariable(
        "SHARP_TARGET_NOOP_VAR2", System::EnvironmentVariableTarget::Machine));
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

// Ticket #2242 resolved two things from the reference tree that #2241 could not.
//
// FIRST, and bigger than the question the ticket asked: PasteArguments is a partial class
// with two Paste implementations, and PasteArguments.Unix.cs states in its own doc-comment
// that "the rules for parsing the executable name (argv[0]) are ignored" -- it routes every
// argument, argv[0] included, through AppendArgument. Only PasteArguments.Windows.cs honours
// the flag. This port applied the argv[0] rules on every platform.
//
// SECOND, the question as asked: under the Windows rules a literal quote in argv[0] is
// REJECTED, not emitted -- `throw new ApplicationException(SR.Argv_IncludeDoubleQuote)`,
// "The argv[0] argument cannot include a double quote." (Strings.resx:4049). #2241's pin,
// which chose to emit it verbatim, was wrong.
//
// The two rule sets agree on everything else that matters here, so three of these four rows
// are byte-identical under both.
TEST(EnvironmentTests, CommandLine_ArgV0_FollowsThePlatformsOwnRuleSet) {
    EXPECT_EQ(commandLineOf({"my prog", "x"}), "\"my prog\" x");
    EXPECT_EQ(commandLineOf({"C:\\dir\\prog", "x"}), "C:\\dir\\prog x");
    EXPECT_EQ(commandLineOf({""}), "\"\"");

#ifdef _WIN32
    // argv[0] rules: no escape exists, so the quote is rejected.
    EXPECT_THROW((void)commandLineOf({"pro\"g", "x"}), System::ApplicationException);
#else
    // No argv[0] rules at all: the quote is escaped exactly as it would be in any other
    // argument, and the result round-trips.
    EXPECT_EQ(commandLineOf({"pro\"g", "x"}), "\"pro\\\"g\" x");
#endif
}

#ifndef _WIN32
// The consequence of routing argv[0] through AppendArgument on Unix is that the WHOLE command
// line round-trips through one parser, argv[0] included -- which the argv[0] rules could not
// promise for a name containing a quote.
TEST(EnvironmentTests, CommandLine_OnUnixTheWholeLineRoundTripsIncludingArgV0) {
    const std::vector<std::vector<std::string>> cases = {
        {"prog", "arg1"},
        {"my prog", "x"},
        {"pro\"g", "x"},
        {"C:\\dir\\prog", "x"},
    };
    for (const auto& argv : cases) {
        const std::string text = commandLineOf(argv);
        EXPECT_EQ(parseCommandLineTail(text), argv) << "emitted: " << text;
    }
}
#endif

TEST(EnvironmentTests, CommandLine_EmptyArgvIsStillEmpty) {
    EXPECT_EQ(commandLineOf({}), "");
}

// ---------------------------------------------------------------------------
// #2320 — the XDG base directories, and SpecialFolderOption on POSIX
// ---------------------------------------------------------------------------

namespace {

/// Saves and restores a variable, so a test cannot leak its setting into the next one.
class ScopedEnv {
public:
    ScopedEnv(const char* name, const char* value) : name_(name) {
        const char* old = std::getenv(name);
        had_ = (old != nullptr);
        if (had_) saved_ = old;
        if (value != nullptr) ::setenv(name, value, 1); else ::unsetenv(name);
    }
    ~ScopedEnv() {
        if (had_) ::setenv(name_, saved_.c_str(), 1); else ::unsetenv(name_);
    }
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;
private:
    const char* name_;
    std::string saved_;
    bool        had_ = false;
};

}  // namespace

TEST(XdgAndFolderOptionTests, Fix2320_AnAbsoluteXdgVariableIsHonoured) {
#if !defined(_WIN32)
    {
        ScopedEnv cfg("XDG_CONFIG_HOME", "/tmp/sharp-runtime-2320-cfg");
        EXPECT_EQ("/tmp/sharp-runtime-2320-cfg",
                  Environment::GetFolderPath(Environment::SpecialFolder::ApplicationData,
                                             Environment::SpecialFolderOption::DoNotVerify));
    }
    {
        ScopedEnv data("XDG_DATA_HOME", "/tmp/sharp-runtime-2320-data");
        EXPECT_EQ("/tmp/sharp-runtime-2320-data",
                  Environment::GetFolderPath(Environment::SpecialFolder::LocalApplicationData,
                                             Environment::SpecialFolderOption::DoNotVerify));
    }
#else
    GTEST_SKIP() << "XDG is a POSIX rule";
#endif
}

TEST(XdgAndFolderOptionTests, Fix2320_ARelativeOrEmptyXdgVariableIsIgnored) {
#if !defined(_WIN32)
    // The reference's test is `config is null || !config.StartsWith('/')`
    // (Environment.GetFolderPathCore.Unix.cs:157-161), NOT "is it set". So a RELATIVE value falls
    // back exactly as an unset one does -- and so does an EMPTY one, because an empty string does
    // not start with '/' either. The XDG specification says the same: a relative value "must be
    // ignored". This is the row a naive `getenv() != nullptr` implementation fails.
    const std::string home = Environment::GetFolderPath(Environment::SpecialFolder::UserProfile,
                                                        Environment::SpecialFolderOption::DoNotVerify);
    ASSERT_FALSE(home.empty());

    for (const char* bad : {"relative/path", "", "~/config", "./config"}) {
        SCOPED_TRACE(bad);
        ScopedEnv cfg("XDG_CONFIG_HOME", bad);
        ScopedEnv data("XDG_DATA_HOME", bad);
        EXPECT_EQ(home + "/.config",
                  Environment::GetFolderPath(Environment::SpecialFolder::ApplicationData,
                                             Environment::SpecialFolderOption::DoNotVerify));
        EXPECT_EQ(home + "/.local/share",
                  Environment::GetFolderPath(Environment::SpecialFolder::LocalApplicationData,
                                             Environment::SpecialFolderOption::DoNotVerify));
    }
#else
    GTEST_SKIP() << "XDG is a POSIX rule";
#endif
}

TEST(XdgAndFolderOptionTests, Fix2320_UnsetXdgFallsBackToTheDocumentedDefaults) {
#if !defined(_WIN32)
    ScopedEnv cfg("XDG_CONFIG_HOME", nullptr);
    ScopedEnv data("XDG_DATA_HOME", nullptr);
    const std::string home = Environment::GetFolderPath(Environment::SpecialFolder::UserProfile,
                                                        Environment::SpecialFolderOption::DoNotVerify);
    EXPECT_EQ(home + "/.config",
              Environment::GetFolderPath(Environment::SpecialFolder::ApplicationData,
                                         Environment::SpecialFolderOption::DoNotVerify));
    EXPECT_EQ(home + "/.local/share",
              Environment::GetFolderPath(Environment::SpecialFolder::LocalApplicationData,
                                         Environment::SpecialFolderOption::DoNotVerify));
#else
    GTEST_SKIP() << "XDG is a POSIX rule";
#endif
}

TEST(XdgAndFolderOptionTests, Fix2320_TheThreeOptionsDifferOnAMissingDirectory) {
#if !defined(_WIN32)
    // Builds its own missing directory rather than hoping one exists, so the row means the same
    // thing in every container.
    ScopedTemporaryDirectory temporary;
    const std::string missing = temporary.path("deep/tree");
    ScopedEnv data("XDG_DATA_HOME", missing.c_str());
    const auto folder = Environment::SpecialFolder::LocalApplicationData;

    // BEFORE #2320 ALL THREE OF THESE RETURNED THE PATH. The option was accepted and ignored on
    // POSIX, so a caller could not tell a real directory from a name.
    EXPECT_EQ(missing, Environment::GetFolderPath(folder, Environment::SpecialFolderOption::DoNotVerify));
    EXPECT_EQ("",      Environment::GetFolderPath(folder, Environment::SpecialFolderOption::None))
        << "the DEFAULT option verifies -- Environment.GetFolderPathCore.Unix.cs:26-40";
    EXPECT_EQ(missing, Environment::GetFolderPath(folder, Environment::SpecialFolderOption::Create));

    // Create really created it, every missing component of it, and the default now agrees.
    EXPECT_EQ(0, ::access(missing.c_str(), R_OK));
    EXPECT_EQ(missing, Environment::GetFolderPath(folder, Environment::SpecialFolderOption::None));
#else
    GTEST_SKIP() << "the POSIX verification path";
#endif
}

TEST(XdgAndFolderOptionTests, Fix2320_AnUndefinedOptionIsRejected) {
    // `Environment.cs:149-163` validates the OPTION even though it deliberately does not validate
    // the FOLDER -- an undefined folder legitimately returns "" (ticket #2321), an undefined
    // option never does.
    EXPECT_THROW((void)Environment::GetFolderPath(Environment::SpecialFolder::ApplicationData,
                                                  static_cast<Environment::SpecialFolderOption>(12345)),
                 System::ArgumentOutOfRangeException);

    // #2321 verified the exception IDENTITY, which was the blocker that deferred this ticket:
    // .NET's call is `new ArgumentOutOfRangeException(nameof(option), option,
    // SR.Format(SR.Arg_EnumIllegalVal, option))`, and Arg_EnumIllegalVal is
    // "Illegal enum value: {0}." (Strings.resx:346). All four parts are asserted, because a
    // transcription that gets the type right and the text wrong is the divergence no later test
    // could distinguish from the real thing.
    try {
        (void)Environment::GetFolderPath(Environment::SpecialFolder::ApplicationData,
                                         static_cast<Environment::SpecialFolderOption>(12345));
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        EXPECT_EQ(e.getParamNameProperty(), "option");
        EXPECT_EQ(e.getActualValueProperty(), "12345");
        const std::string message = e.what();
        EXPECT_NE(message.find("Illegal enum value: 12345."), std::string::npos) << message;
        EXPECT_NE(message.find("option"), std::string::npos) << message;
        // The three-argument form is what adds this clause, and #2320 had used the two-argument
        // one -- the only part of the transcription that was wrong.
        EXPECT_NE(message.find("Actual value was 12345."), std::string::npos) << message;
    }

    // ...and all three defined ones are accepted, so the check is a definedness test rather than
    // an accidental range test.
    for (auto option : {Environment::SpecialFolderOption::None,
                        Environment::SpecialFolderOption::DoNotVerify,
                        Environment::SpecialFolderOption::Create}) {
        EXPECT_NO_THROW((void)Environment::GetFolderPath(Environment::SpecialFolder::UserProfile, option));
    }
}

// #2321 answered. Its C4 asked for a "definedness table" separating defined-but-unmapped folders
// from undefined ones. THE REFERENCE SAYS NO SUCH TABLE IS NEEDED HERE, and says why in a
// comment of its own: "No need to validate if 'folder' is defined; GetSpecialFolder handles this
// check" (GetFolderPathCore.Unix.cs:22). GetSpecialFolder is a switch returning `null` for
// anything unhandled, and GetFolderPathCore turns null into "" (:24). So on POSIX the two
// categories are deliberately indistinguishable, and both answer "".
//
// .NET's WINDOWS core does the opposite -- its switch ends in
// `throw new ArgumentOutOfRangeException(nameof(folder), folder, ...)`
// (Environment.Windows.cs:768-770). That divergence is real, is unreachable from this gate, and
// is ticket #2378.
// #2378, split out of #2321. The BEHAVIOUR it governs is Windows-only -- .NET's Windows core
// throws for an undefined SpecialFolder where its Unix core returns "" -- but the DEFINEDNESS
// TABLE is platform-independent data, and a typo in it is the failure mode worth catching. So the
// table is tested here, on the platform this repository's gate actually runs, and the Windows arm
// that consumes it is compile-verified separately (see the ticket).
TEST(XdgAndFolderOptionTests, Fix2378_TheDefinednessTableIsTheEnumsOwnDeclaration) {
    using SF = Environment::SpecialFolder;

    // Every enumerator is defined. Listing them by name rather than by value is what makes this a
    // check of the TABLE against the ENUM, instead of the table against itself.
    for (SF f : {SF::Desktop, SF::Programs, SF::MyDocuments, SF::Personal, SF::Favorites,
                 SF::Startup, SF::Recent, SF::SendTo, SF::StartMenu, SF::MyMusic, SF::MyVideos,
                 SF::DesktopDirectory, SF::MyComputer, SF::NetworkShortcuts, SF::Fonts,
                 SF::Templates, SF::CommonStartMenu, SF::CommonPrograms, SF::CommonStartup,
                 SF::CommonDesktopDirectory, SF::ApplicationData, SF::PrinterShortcuts,
                 SF::LocalApplicationData, SF::InternetCache, SF::Cookies, SF::History,
                 SF::CommonApplicationData, SF::Windows, SF::System, SF::ProgramFiles,
                 SF::MyPictures, SF::UserProfile, SF::SystemX86, SF::ProgramFilesX86,
                 SF::CommonProgramFiles, SF::CommonProgramFilesX86, SF::CommonTemplates,
                 SF::CommonDocuments, SF::CommonAdminTools, SF::AdminTools, SF::CommonMusic,
                 SF::CommonPictures, SF::CommonVideos, SF::Resources, SF::LocalizedResources,
                 SF::CommonOemLinks, SF::CDBurning}) {
        EXPECT_TRUE(System::detail::IsDefinedSpecialFolder(f))
            << "value " << static_cast<int>(f);
    }

    // The 14 holes inside 0x00-0x3B, which is the half a table written by hand gets wrong. A
    // predicate that answered "defined" for a contiguous range would pass every row above.
    for (int hole : {0x01, 0x03, 0x04, 0x0A, 0x0C, 0x0F, 0x12, 0x1D, 0x1E, 0x1F,
                     0x31, 0x32, 0x33, 0x34}) {
        EXPECT_FALSE(System::detail::IsDefinedSpecialFolder(static_cast<SF>(hole))) << hole;
    }

    // ...and everything outside the range.
    for (int outside : {-1, 0x3C, 0x40, 0x100, 12345}) {
        EXPECT_FALSE(System::detail::IsDefinedSpecialFolder(static_cast<SF>(outside))) << outside;
    }

    // The counts the ticket recorded, asserted rather than trusted: 47 enumerators over 46
    // distinct values, because Personal and MyDocuments share 0x05.
    EXPECT_EQ(static_cast<int>(SF::Personal), static_cast<int>(SF::MyDocuments));
    int defined = 0;
    for (int v = 0; v <= 0x3B; ++v) {
        if (System::detail::IsDefinedSpecialFolder(static_cast<SF>(v))) ++defined;
    }
    EXPECT_EQ(defined, 46);
}

// The POSIX arm is UNCHANGED by #2378, and saying so needs its own row: the table exists on this
// platform too, and a careless implementation would have used it here.
TEST(XdgAndFolderOptionTests, Fix2378_ThePosixAnswerIsUntouched) {
#ifndef _WIN32
    for (int raw : {12345, -1, 0x0100, 0x003C, 0x0001}) {
        const auto folder = static_cast<Environment::SpecialFolder>(raw);
        EXPECT_FALSE(System::detail::IsDefinedSpecialFolder(folder)) << raw;
        std::string path;
        EXPECT_NO_THROW(path = Environment::GetFolderPath(folder)) << raw;
        EXPECT_TRUE(path.empty()) << raw;
    }
#else
    GTEST_SKIP() << "Windows throws instead; that arm is verified by cross-compilation";
#endif
}

TEST(XdgAndFolderOptionTests, Fix2321_AnUndefinedFolderReturnsEmptyOnPosixRatherThanThrowing) {
#ifndef _WIN32
    for (int raw : {12345, -1, 0x0100, 0x003C}) {
        const auto folder = static_cast<Environment::SpecialFolder>(raw);
        std::string path;
        EXPECT_NO_THROW(path = Environment::GetFolderPath(folder)) << raw;
        EXPECT_TRUE(path.empty()) << raw << " -> " << path;
        // ...and the option is still validated first, so the two checks do not interfere.
        EXPECT_THROW((void)Environment::GetFolderPath(folder,
                                                      static_cast<Environment::SpecialFolderOption>(999)),
                     System::ArgumentOutOfRangeException) << raw;
    }

    // The property that makes the above a transcription rather than a shrug: a DEFINED folder
    // this platform does not map gives the SAME answer as an undefined one. .NET makes them
    // indistinguishable on purpose, so a test that only probed undefined values would pass
    // against a definedness table that does not exist.
    EXPECT_TRUE(Environment::GetFolderPath(Environment::SpecialFolder::Cookies).empty());
    EXPECT_TRUE(Environment::GetFolderPath(Environment::SpecialFolder::SendTo).empty());
    EXPECT_TRUE(Environment::GetFolderPath(Environment::SpecialFolder::StartMenu).empty());
#else
    GTEST_SKIP() << "the POSIX answer; Windows throws instead -- ticket #2378";
#endif
}
