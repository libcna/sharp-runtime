// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "System/ArgumentException.hpp"
#include "System/Diagnostics/Process.hpp"

using System::Diagnostics::Process;
using System::Diagnostics::ProcessStartInfo;

namespace {
constexpr const char* EnvironmentKey = "SHARP_RUNTIME_PROCESS_ENV_TEST_1763";
}

TEST(ProcessEnvironmentOverrideTest, ChildReceivesOverrideWithoutChangingParentEnvironment) {
    const char* before = std::getenv(EnvironmentKey);
    const std::string beforeValue = before == nullptr ? "" : before;
    const bool hadBeforeValue = before != nullptr;

    ProcessStartInfo startInfo("/bin/sh");
    startInfo.getArgumentListProperty().push_back("-c");
    startInfo.getArgumentListProperty().push_back("printf '%s' \"$SHARP_RUNTIME_PROCESS_ENV_TEST_1763\"");
    startInfo.getEnvironmentVariablesProperty()[EnvironmentKey] = "child-value";
    startInfo.setRedirectStandardOutputProperty(true);
    Process process = Process::Start(startInfo);
    process.WaitForExit();

    EXPECT_EQ(process.getExitCodeProperty(), 0);
    EXPECT_EQ(process.getStandardOutputTextProperty(), "child-value");
    const char* after = std::getenv(EnvironmentKey);
    EXPECT_EQ(after != nullptr, hadBeforeValue);
    if (after != nullptr) {
        EXPECT_EQ(std::string(after), beforeValue);
    }
}

TEST(ProcessEnvironmentOverrideTest, EmptyValueIsPassedLiterallyToChild) {
    ProcessStartInfo startInfo("/bin/sh");
    startInfo.getArgumentListProperty().push_back("-c");
    startInfo.getArgumentListProperty().push_back("test -z \"$SHARP_RUNTIME_PROCESS_ENV_TEST_1763\"");
    startInfo.getEnvironmentVariablesProperty()[EnvironmentKey] = "";
    Process process = Process::Start(startInfo);
    process.WaitForExit();

    EXPECT_EQ(process.getExitCodeProperty(), 0);
}

TEST(ProcessEnvironmentOverrideTest, InvalidVariableNamesAreRejectedBeforeForking) {
    ProcessStartInfo startInfo("/bin/true");
    startInfo.getEnvironmentVariablesProperty()["INVALID=NAME"] = "value";

    EXPECT_THROW(Process::Start(startInfo), System::ArgumentException);
}
