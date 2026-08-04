// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2025 (SR-AUD-270, cause D-C of docs/SystemDiagnosticsNamespaceReviewPlan.md).
//
// Process::Start() on an existing instance is the restart its own doc-comment advertises --
// and it is the ONLY way to call the instance overload at all, because the default
// constructor is private, so every public instance Start() is a restart.
//
// Before this ticket, that path assigned into a joinable std::thread, which calls
// std::terminate: the whole process died with SIGABRT. Measured in
// build-probe/2025_probe1_before.log, the trigger was a joinable READER thread rather than a
// running child, so a restart after the previous child had already exited aborted too
// whenever the caller had not called WaitForExit()/getHasExitedProperty() first (case C).
//
// Every test here must leave no child behind: a refused restart keeps the previous child
// owned by the object, so these tests kill and wait for it explicitly.

#include <gtest/gtest.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <string>
#include <thread>

#include "System/Diagnostics/Process.hpp"
#include "System/InvalidOperationException.hpp"

using System::Diagnostics::Process;
using System::Diagnostics::ProcessStartInfo;

namespace {

ProcessStartInfo shellStartInfo(const std::string& script, bool redirectOut, bool redirectErr) {
    ProcessStartInfo startInfo("/bin/sh");
    startInfo.getArgumentListProperty().push_back("-c");
    startInfo.getArgumentListProperty().push_back(script);
    startInfo.setRedirectStandardOutputProperty(redirectOut);
    startInfo.setRedirectStandardErrorProperty(redirectErr);
    return startInfo;
}

// Terminates whatever the instance currently owns and reaps it, so no test leaves a zombie.
void killAndReap(Process& process) {
    process.Kill();
    process.WaitForExit();
}

} // namespace

// ---------------------------------------------------------------------------
// Restart after the previous child exited -- must work, in every combination.
// ---------------------------------------------------------------------------

TEST(ProcessRestartTests, RestartAfterExit_Unredirected_Works) {
    Process process = Process::Start(shellStartInfo("exit 3", false, false));
    process.WaitForExit();
    ASSERT_EQ(process.getExitCodeProperty(), 3);

    process.setStartInfoProperty(shellStartInfo("exit 4", false, false));
    ASSERT_TRUE(process.Start());
    process.WaitForExit();
    EXPECT_EQ(process.getExitCodeProperty(), 4);
}

TEST(ProcessRestartTests, RestartAfterExit_Redirected_Works) {
    Process process = Process::Start(shellStartInfo("printf first", true, false));
    process.WaitForExit();
    ASSERT_EQ(process.getStandardOutputTextProperty(), "first");

    process.setStartInfoProperty(shellStartInfo("printf second", true, false));
    ASSERT_TRUE(process.Start());
    process.WaitForExit();
    EXPECT_EQ(process.getExitCodeProperty(), 0);
}

// The exact case that aborted before #2025: the child has exited, but nothing joined the
// reader thread, because only WaitForExit()/getHasExitedProperty() do that.
TEST(ProcessRestartTests, RestartAfterExit_WithoutWaiting_DoesNotAbort) {
    Process process = Process::Start(shellStartInfo("printf x", true, false));
    // Deliberately do NOT call WaitForExit() or getHasExitedProperty() here.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    process.setStartInfoProperty(shellStartInfo("printf y", true, false));
    ASSERT_TRUE(process.Start());
    process.WaitForExit();
    EXPECT_EQ(process.getStandardOutputTextProperty(), "y");
}

TEST(ProcessRestartTests, RestartAfterExit_BothStreamsRedirected_Works) {
    Process process = Process::Start(
        shellStartInfo("printf out; printf err 1>&2", true, true));
    process.WaitForExit();
    ASSERT_EQ(process.getStandardOutputTextProperty(), "out");
    ASSERT_EQ(process.getStandardErrorTextProperty(), "err");

    process.setStartInfoProperty(shellStartInfo("printf OUT; printf ERR 1>&2", true, true));
    ASSERT_TRUE(process.Start());
    process.WaitForExit();
    EXPECT_EQ(process.getStandardOutputTextProperty(), "OUT");
    EXPECT_EQ(process.getStandardErrorTextProperty(), "ERR");
}

TEST(ProcessRestartTests, DoubleRestart_Works) {
    Process process = Process::Start(shellStartInfo("exit 1", false, false));
    process.WaitForExit();

    process.setStartInfoProperty(shellStartInfo("exit 2", false, false));
    ASSERT_TRUE(process.Start());
    process.WaitForExit();
    ASSERT_EQ(process.getExitCodeProperty(), 2);

    process.setStartInfoProperty(shellStartInfo("exit 5", false, false));
    ASSERT_TRUE(process.Start());
    process.WaitForExit();
    EXPECT_EQ(process.getExitCodeProperty(), 5);
}

// ---------------------------------------------------------------------------
// The captured text belongs to the process that produced it.
// ---------------------------------------------------------------------------

TEST(ProcessRestartTests, RestartResetsCapturedStandardOutput) {
    Process process = Process::Start(shellStartInfo("printf first", true, false));
    process.WaitForExit();
    ASSERT_EQ(process.getStandardOutputTextProperty(), "first");

    process.setStartInfoProperty(shellStartInfo("printf second", true, false));
    process.Start();
    process.WaitForExit();
    // Before #2025 this accumulated into "firstsecond".
    EXPECT_EQ(process.getStandardOutputTextProperty(), "second");
}

TEST(ProcessRestartTests, RestartResetsCapturedStandardError) {
    Process process = Process::Start(shellStartInfo("printf first 1>&2", false, true));
    process.WaitForExit();
    ASSERT_EQ(process.getStandardErrorTextProperty(), "first");

    process.setStartInfoProperty(shellStartInfo("printf second 1>&2", false, true));
    process.Start();
    process.WaitForExit();
    EXPECT_EQ(process.getStandardErrorTextProperty(), "second");
}

// ---------------------------------------------------------------------------
// Restart while the previous child is still running -- refused, never aborted.
// ---------------------------------------------------------------------------

TEST(ProcessRestartTests, RestartWhileRunning_Redirected_Throws) {
    Process process = Process::Start(shellStartInfo("exec sleep 30", true, false));
    process.setStartInfoProperty(shellStartInfo("printf z", true, false));

    EXPECT_THROW(process.Start(), System::InvalidOperationException);

    killAndReap(process);
}

TEST(ProcessRestartTests, RestartWhileRunning_Unredirected_Throws) {
    Process process = Process::Start(shellStartInfo("exec sleep 30", false, false));
    process.setStartInfoProperty(shellStartInfo("exit 0", false, false));

    EXPECT_THROW(process.Start(), System::InvalidOperationException);

    killAndReap(process);
}

TEST(ProcessRestartTests, RestartWhileRunning_BothStreamsRedirected_Throws) {
    Process process = Process::Start(shellStartInfo("exec sleep 30", true, true));
    process.setStartInfoProperty(shellStartInfo("exit 0", true, true));

    EXPECT_THROW(process.Start(), System::InvalidOperationException);

    killAndReap(process);
}

// A refused restart must leave the instance describing the process it already owns, so the
// caller can still wait for and kill it -- no partial state transition.
TEST(ProcessRestartTests, RefusedRestart_LeavesPreviousProcessUsable) {
    Process process = Process::Start(shellStartInfo("exec sleep 30", false, false));
    const auto originalId = process.getIdProperty();
    process.setStartInfoProperty(shellStartInfo("exit 0", false, false));

    EXPECT_THROW(process.Start(), System::InvalidOperationException);

    EXPECT_EQ(process.getIdProperty(), originalId);
    EXPECT_FALSE(process.getHasExitedProperty());

    process.Kill();
    ASSERT_TRUE(process.WaitForExit(5000));
    EXPECT_TRUE(process.getHasExitedProperty());
}

TEST(ProcessRestartTests, RestartAfterKillAndWait_Works) {
    Process process = Process::Start(shellStartInfo("exec sleep 30", false, false));
    process.Kill();
    ASSERT_TRUE(process.WaitForExit(5000));

    process.setStartInfoProperty(shellStartInfo("exit 6", false, false));
    ASSERT_TRUE(process.Start());
    process.WaitForExit();
    EXPECT_EQ(process.getExitCodeProperty(), 6);
}

// ---------------------------------------------------------------------------
// A failed restart must not damage the instance.
// ---------------------------------------------------------------------------

TEST(ProcessRestartTests, RestartAfterFailedRestart_Works) {
    Process process = Process::Start(shellStartInfo("exit 1", false, false));
    process.WaitForExit();

    process.setStartInfoProperty(ProcessStartInfo("/definitely-missing-sharp-runtime-2025"));
    EXPECT_THROW(process.Start(), System::InvalidOperationException);

    process.setStartInfoProperty(shellStartInfo("exit 7", false, false));
    ASSERT_TRUE(process.Start());
    process.WaitForExit();
    EXPECT_EQ(process.getExitCodeProperty(), 7);
}

TEST(ProcessRestartTests, RestartWithEmptyFileName_ThrowsAndLeavesInstanceUsable) {
    Process process = Process::Start(shellStartInfo("exit 1", false, false));
    process.WaitForExit();

    process.setStartInfoProperty(ProcessStartInfo(""));
    EXPECT_THROW(process.Start(), System::InvalidOperationException);

    process.setStartInfoProperty(shellStartInfo("exit 8", false, false));
    ASSERT_TRUE(process.Start());
    process.WaitForExit();
    EXPECT_EQ(process.getExitCodeProperty(), 8);
}

// ---------------------------------------------------------------------------
// No test above may leave a child of this test process unreaped.
// ---------------------------------------------------------------------------

TEST(ProcessRestartTests, ZZZ_NoZombieChildrenRemain) {
    // Any child this suite forgot to reap would still be waitable here. waitpid returns -1
    // with ECHILD when the caller has no unwaited-for children left, which is the state a
    // leak-free suite must be in.
    int status = 0;
    pid_t reaped = ::waitpid(-1, &status, WNOHANG);
    EXPECT_EQ(reaped, -1) << "an unreaped child process survived this suite";
    EXPECT_EQ(errno, ECHILD);
}
