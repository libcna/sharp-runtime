// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2024 (SR-AUD-268 + SR-AUD-272, cause D-A of
// docs/SystemDiagnosticsNamespaceReviewPlan.md). One test per row of the plan's section 7.3.
//
// Measured before the repair (build-probe/2024_probe1_before.log):
//   * WaitForExit(-1) on a 2 s child returned FALSE after 0 ms, although .NET's -1 is
//     Timeout.Infinite -- the deadline was computed in the past, so the loop body never ran;
//   * WaitForExit(-2) and WaitForExit(INTCS_MIN) returned normally with no diagnostic;
//   * WaitForExit(), whose contract is "blocks the calling thread until the associated
//     process terminates", returned after 1000 ms on a 3 s child when a non-SA_RESTART
//     SIGALRM interrupted the underlying waitpid, and getExitCodeProperty() then threw.
//
// The children below use `exec sleep N` deliberately: /bin/sh is dash, which FORKS for a plain
// `sleep N`, and a surviving grandchild holding the redirected pipe used to make WaitForExit
// block past its own deadline. Ticket #2032 closed that follow-up by making bounded waits skip
// the unbounded reader-thread join; these deadline tests still avoid introducing a descendant.

#include <gtest/gtest.h>

#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Diagnostics/Process.hpp"
#include "System/InvalidOperationException.hpp"

using SharpRuntime::intcs;
using System::Diagnostics::Process;
using System::Diagnostics::ProcessStartInfo;

namespace {

volatile sig_atomic_t alarmCount = 0;
extern "C" void countAlarm(int) { alarmCount = alarmCount + 1; }

ProcessStartInfo shellStartInfo(const std::string& script) {
    ProcessStartInfo startInfo("/bin/sh");
    startInfo.getArgumentListProperty().push_back("-c");
    startInfo.getArgumentListProperty().push_back(script);
    return startInfo;
}

long elapsedMsSince(std::chrono::steady_clock::time_point start) {
    return static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count());
}

} // namespace

// ---------------------------------------------------------------------------
// -1 is Timeout.Infinite.
// ---------------------------------------------------------------------------

TEST(ProcessWaitForExitTests, NegativeOne_WaitsIndefinitelyAndReturnsTrue) {
    Process process = Process::Start(shellStartInfo("exec sleep 1"));

    const auto started = std::chrono::steady_clock::now();
    const bool exited = process.WaitForExit(-1);
    const long waited = elapsedMsSince(started);

    EXPECT_TRUE(exited);
    EXPECT_GE(waited, 900) << "WaitForExit(-1) returned before the child could have exited";
    EXPECT_TRUE(process.getHasExitedProperty());
    EXPECT_EQ(process.getExitCodeProperty(), 0);
}

TEST(ProcessWaitForExitTests, NegativeOne_OnAnAlreadyExitedChild_ReturnsTrueImmediately) {
    Process process = Process::Start(shellStartInfo("exit 0"));
    process.WaitForExit();

    const auto started = std::chrono::steady_clock::now();
    EXPECT_TRUE(process.WaitForExit(-1));
    EXPECT_LT(elapsedMsSince(started), 500);
}

// ---------------------------------------------------------------------------
// Anything below -1 is rejected.
// ---------------------------------------------------------------------------

TEST(ProcessWaitForExitTests, BelowNegativeOne_ThrowsArgumentOutOfRange) {
    Process process = Process::Start(shellStartInfo("exit 0"));
    process.WaitForExit();

    for (const intcs invalid : {static_cast<intcs>(-2), static_cast<intcs>(-1000),
                                SharpRuntime::INTCS_MIN}) {
        try {
            process.WaitForExit(invalid);
            FAIL() << "WaitForExit(" << invalid << ") did not throw";
        } catch (const System::ArgumentOutOfRangeException& error) {
            EXPECT_EQ(error.getParamNameProperty(), "milliseconds");
            EXPECT_NE(error.getMessageProperty().find(
                          "Number must be either non-negative and less than or equal to "
                          "Int32.MaxValue or -1."),
                      std::string::npos);
        }
    }
}

// The argument is validated before any state is consulted, so a still-running child does not
// change which exception a caller sees.
TEST(ProcessWaitForExitTests, BelowNegativeOne_ThrowsEvenWhileTheChildRuns) {
    Process process = Process::Start(shellStartInfo("exec sleep 30"));

    EXPECT_THROW(process.WaitForExit(-2), System::ArgumentOutOfRangeException);
    EXPECT_FALSE(process.getHasExitedProperty());

    process.Kill();
    ASSERT_TRUE(process.WaitForExit(5000));
}

// ---------------------------------------------------------------------------
// The rows that must not change.
// ---------------------------------------------------------------------------

TEST(ProcessWaitForExitTests, Zero_PollsOnceOnALiveChild) {
    Process process = Process::Start(shellStartInfo("exec sleep 30"));

    const auto started = std::chrono::steady_clock::now();
    EXPECT_FALSE(process.WaitForExit(0));
    EXPECT_LT(elapsedMsSince(started), 500);

    process.Kill();
    ASSERT_TRUE(process.WaitForExit(5000));
}

TEST(ProcessWaitForExitTests, Zero_ReturnsTrueOnAnExitedChild) {
    Process process = Process::Start(shellStartInfo("exit 0"));
    process.WaitForExit();
    EXPECT_TRUE(process.WaitForExit(0));
}

TEST(ProcessWaitForExitTests, PositiveTimeout_ThatDoesNotExpire_ReturnsTrue) {
    Process process = Process::Start(shellStartInfo("exit 0"));
    EXPECT_TRUE(process.WaitForExit(5000));
    EXPECT_EQ(process.getExitCodeProperty(), 0);
}

TEST(ProcessWaitForExitTests, PositiveTimeout_ThatExpires_ReturnsFalse) {
    Process process = Process::Start(shellStartInfo("exec sleep 30"));

    const auto started = std::chrono::steady_clock::now();
    EXPECT_FALSE(process.WaitForExit(200));
    EXPECT_GE(elapsedMsSince(started), 150);

    process.Kill();
    ASSERT_TRUE(process.WaitForExit(5000));
}

TEST(ProcessWaitForExitTests, MaxValueTimeout_OnAFastChild_ReturnsPromptly) {
    // The deadline arithmetic must stay defined at the top of the domain too.
    Process process = Process::Start(shellStartInfo("exit 0"));

    const auto started = std::chrono::steady_clock::now();
    EXPECT_TRUE(process.WaitForExit(SharpRuntime::INTCS_MAX));
    EXPECT_LT(elapsedMsSince(started), 5000);
}

TEST(ProcessWaitForExitTests, GetCurrentProcess_IsUnchanged) {
    Process self = Process::GetCurrentProcess();

    const auto started = std::chrono::steady_clock::now();
    EXPECT_FALSE(self.WaitForExit(-1));
    EXPECT_FALSE(self.WaitForExit(0));
    self.WaitForExit();
    EXPECT_LT(elapsedMsSince(started), 500)
        << "WaitForExit on the current process must not block";
}

// ---------------------------------------------------------------------------
// SR-AUD-272: the blocking overload honours its own contract under EINTR.
// ---------------------------------------------------------------------------

TEST(ProcessWaitForExitTests, BlockingWait_IsNotCutShortByAnInterruptingSignal) {
    // Installed with sigaction and WITHOUT SA_RESTART on purpose: glibc's signal() installs a
    // BSD-semantics handler that sets SA_RESTART, which would not reproduce the defect at all.
    struct sigaction handler {};
    handler.sa_handler = countAlarm;
    ::sigemptyset(&handler.sa_mask);
    handler.sa_flags = 0;
    struct sigaction previous {};
    ASSERT_EQ(::sigaction(SIGALRM, &handler, &previous), 0);

    Process process = Process::Start(shellStartInfo("exec sleep 2"));
    alarmCount = 0;
    ::alarm(1);

    const auto started = std::chrono::steady_clock::now();
    process.WaitForExit();
    const long waited = elapsedMsSince(started);

    ::alarm(0);
    ASSERT_EQ(::sigaction(SIGALRM, &previous, nullptr), 0);

    EXPECT_GE(alarmCount, 1) << "the signal was never delivered, so nothing was proven";
    EXPECT_GE(waited, 1800) << "WaitForExit() returned while the child was still running";
    EXPECT_TRUE(process.getHasExitedProperty());
    EXPECT_EQ(process.getExitCodeProperty(), 0);
}

TEST(ProcessWaitForExitTests, TimedWait_IsNotCutShortByAnInterruptingSignal) {
    struct sigaction handler {};
    handler.sa_handler = countAlarm;
    ::sigemptyset(&handler.sa_mask);
    handler.sa_flags = 0;
    struct sigaction previous {};
    ASSERT_EQ(::sigaction(SIGALRM, &handler, &previous), 0);

    Process process = Process::Start(shellStartInfo("exec sleep 2"));
    alarmCount = 0;
    ::alarm(1);

    const bool exited = process.WaitForExit(-1);

    ::alarm(0);
    ASSERT_EQ(::sigaction(SIGALRM, &previous, nullptr), 0);

    EXPECT_GE(alarmCount, 1);
    EXPECT_TRUE(exited);
    EXPECT_EQ(process.getExitCodeProperty(), 0);
}

TEST(ProcessWaitForExitTests, ZZZ_NoZombieChildrenRemain) {
    int status = 0;
    pid_t reaped = ::waitpid(-1, &status, WNOHANG);
    EXPECT_EQ(reaped, -1) << "an unreaped child process survived this suite";
    EXPECT_EQ(errno, ECHILD);
}
