// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/Environment.hpp"

using System::Environment;
using SharpRuntime::longcs;

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
    bool v = Environment::Is64BitProcess();
    (void)v;
}

TEST(EnvironmentTests, Is64BitOperatingSystem_ReturnsBool) {
    bool v = Environment::Is64BitOperatingSystem();
    (void)v;
}

TEST(EnvironmentTests, Is64BitOS_EqualsIs64BitProcess) {
    // This implementation delegates OS check to process check
    EXPECT_EQ(Environment::Is64BitOperatingSystem(), Environment::Is64BitProcess());
}

// ---------------------------------------------------------------------------
// MachineName / UserName / TickCount64
// ---------------------------------------------------------------------------
TEST(EnvironmentTests, MachineName_NotEmpty) {
    std::string name = Environment::getMachineNameProperty();
    EXPECT_FALSE(name.empty());
}

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
    volatile int sink = 0;
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
