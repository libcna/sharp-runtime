// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors

#include <cerrno>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include "System/Diagnostics/Process.hpp"
#include "System/InvalidOperationException.hpp"

using System::Diagnostics::Process;
using System::Diagnostics::ProcessStartInfo;

namespace {

void expectStartupErrorFor(const ProcessStartInfo& startInfo, const std::string& expectedPath) {
    try {
        (void)Process::Start(startInfo);
        FAIL() << "Process::Start unexpectedly succeeded";
    } catch (const System::InvalidOperationException& error) {
        const std::string message = error.getMessageProperty();
        EXPECT_NE(message.find(expectedPath), std::string::npos);
        EXPECT_NE(message.find(std::strerror(ENOENT)), std::string::npos);
    }
}

} // namespace

TEST(ProcessStartupFailureTest, MissingExecutableThrowsSynchronously) {
    const std::string missingExecutable = "/definitely-missing-sharp-runtime-process-1764";
    expectStartupErrorFor(ProcessStartInfo(missingExecutable), missingExecutable);
}

TEST(ProcessStartupFailureTest, MissingWorkingDirectoryThrowsSynchronously) {
    const std::string missingDirectory = "/definitely-missing-sharp-runtime-directory-1764";
    ProcessStartInfo startInfo("/bin/true");
    startInfo.setWorkingDirectoryProperty(missingDirectory);
    expectStartupErrorFor(startInfo, "/bin/true");
}

TEST(ProcessStartupFailureTest, SuccessfulLaunchStillReturnsRunningProcess) {
    ProcessStartInfo startInfo("/bin/sh");
    startInfo.getArgumentListProperty().push_back("-c");
    startInfo.getArgumentListProperty().push_back("printf startup-ok");
    startInfo.setRedirectStandardOutputProperty(true);

    Process process = Process::Start(startInfo);
    process.WaitForExit();

    EXPECT_EQ(process.getExitCodeProperty(), 0);
    EXPECT_EQ(process.getStandardOutputTextProperty(), "startup-ok");
}
