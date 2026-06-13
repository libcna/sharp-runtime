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
