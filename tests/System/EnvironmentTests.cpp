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
