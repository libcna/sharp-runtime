// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2033 -- PINS for the reader-thread join, which is reached from FIVE public doors.
//
// Every test in this file asserts behaviour that is KNOWN TO BE WRONG and is deliberately left
// unrepaired because repairing it means choosing what a pipe-reader thread that cannot finish
// should do -- join it (today, unbounded), detach it, or abandon the descriptor. That choice is
// the policy ticket #2029 gates (docs/SystemDiagnosticsNamespaceReviewPlan.md section 14.1), and
// ticket #2032 records its most visible consequence: WaitForExit(milliseconds) exceeding its own
// bound. NOTHING HERE IMPLEMENTS OR AUTHORISES EITHER.
//
// Why this file exists separately from ProcessGatedBehaviourPinTests.cpp: #2032 and the header
// both named only WaitForExit(intcs). Measured 2026-08-04 against an 8 s grandchild holding the
// redirected pipe (build-probe/2033_probe1_reader_join_entry_points.log):
//
//     WaitForExit(500)        7502 ms   -- against a declared 500 ms bound
//     getHasExitedProperty()  7502 ms   -- a const "poll" getter, NO bound exists
//     Kill(false)             7502 ms   -- "immediately stops", NO bound exists
//     Start()  (restart)      7503 ms   -- NO bound exists
//     ~Process                8004 ms   -- SR-AUD-269's blocking half, pinned separately
//
// So a repair applied to the destructor alone would leave four public doors unbounded. These
// pins make that visible: the moment #2029's policy lands on any of them, the corresponding test
// here fails, which is the signal this file exists to produce, not a regression to silence.
//
// Each pin uses a SHORT keep-alive so the suite stays fast; the shape, not the magnitude, is
// what is being pinned. The lower bounds are deliberately well under the keep-alive so a loaded
// machine cannot make them flaky.

#include <gtest/gtest.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <string>
#include <thread>

#include "System/Diagnostics/Process.hpp"
#include "System/Diagnostics/ProcessStartInfo.hpp"

using System::Diagnostics::Process;
using System::Diagnostics::ProcessStartInfo;

namespace {

// How long the grandchild holds the inherited write end of the redirected stdout pipe.
constexpr int kKeepAliveSeconds = 3;

// The lower bound each pinned call must exceed. Well below kKeepAliveSeconds so scheduling noise
// cannot fail the test, and well above the milliseconds a non-blocking implementation would take.
constexpr long kBlockedFloorMs = 1500;

// /bin/sh is dash in this container, which FORKS rather than exec's for `sleep N`. That is the
// point: the direct child is a shell whose own child inherits the redirected stdout and outlives
// it, so the pipe stays open after the direct child is gone. (The other Process suites use
// `exec sleep N` precisely to AVOID this shape; here it is the subject.)
ProcessStartInfo grandchildHoldsThePipe() {
    ProcessStartInfo startInfo("/bin/sh");
    startInfo.getArgumentListProperty().push_back("-c");
    startInfo.getArgumentListProperty().push_back("sleep " + std::to_string(kKeepAliveSeconds));
    startInfo.setRedirectStandardOutputProperty(true);
    return startInfo;
}

// Kills the DIRECT child without going through Process::Kill(), which is itself one of the pinned
// doors, and waits for the signal to land. The first sleep gives the shell time to fork its own
// `sleep`, so afterwards the direct child is dead-but-unreaped while the grandchild holds the pipe.
void killDirectChildAndSettle(Process& process) {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    ::kill(static_cast<pid_t>(process.getIdProperty()), SIGKILL);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

long elapsedMsSince(std::chrono::steady_clock::time_point start) {
    return static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count());
}

} // namespace

// PIN FOR BLOCKED TICKETS #2032 / #2029. WaitForExit(milliseconds) exceeds its own declared bound
// because reaping the child joins the readers. This is #2032's own shape at a smaller scale.
TEST(ProcessReaderJoinBlockingPinTests, Pin2032_WaitForExitTimeoutIsExceededBehindAGrandchild) {
    Process process = Process::Start(grandchildHoldsThePipe());
    killDirectChildAndSettle(process);

    const auto started = std::chrono::steady_clock::now();
    const bool exited = process.WaitForExit(200);
    const long blockedMs = elapsedMsSince(started);

    EXPECT_TRUE(exited);
    EXPECT_GE(blockedMs, kBlockedFloorMs)
        << "WaitForExit(200) honoured its bound -- #2032/#2029 appear to have landed; retire this pin";
}

// PIN FOR BLOCKED TICKETS #2032 / #2029, on a door #2032 does not name. getHasExitedProperty() is
// documented as reading whether the process has terminated and takes no timeout at all, so unlike
// WaitForExit(ms) it has no bound to exceed -- it simply blocks for as long as a descendant holds
// the pipe.
TEST(ProcessReaderJoinBlockingPinTests, Pin2033_HasExitedBlocksBehindAGrandchild) {
    Process process = Process::Start(grandchildHoldsThePipe());
    killDirectChildAndSettle(process);

    const auto started = std::chrono::steady_clock::now();
    const bool exited = process.getHasExitedProperty();
    const long blockedMs = elapsedMsSince(started);

    EXPECT_TRUE(exited);
    EXPECT_GE(blockedMs, kBlockedFloorMs)
        << "getHasExitedProperty() returned promptly -- the reader-join policy (#2029) appears to "
           "have landed; retire this pin";
}

// PIN FOR BLOCKED TICKETS #2032 / #2029, second unnamed door. Kill() is documented as immediately
// stopping the process; it first observes whether the child has already exited, which reaps it and
// joins the readers.
TEST(ProcessReaderJoinBlockingPinTests, Pin2033_KillBlocksBehindAGrandchild) {
    Process process = Process::Start(grandchildHoldsThePipe());
    killDirectChildAndSettle(process);

    const auto started = std::chrono::steady_clock::now();
    process.Kill();
    const long blockedMs = elapsedMsSince(started);

    EXPECT_GE(blockedMs, kBlockedFloorMs)
        << "Kill() returned promptly -- the reader-join policy (#2029) appears to have landed; "
           "retire this pin";
}

// PIN FOR BLOCKED TICKETS #2032 / #2029, third unnamed door. #2025's restart preamble resolves the
// previous child's readers before assigning over the std::thread, so a restart inherits the same
// unbounded wait. The restart itself is correct and must keep working -- only its duration is pinned.
TEST(ProcessReaderJoinBlockingPinTests, Pin2033_RestartBlocksBehindAGrandchild) {
    Process process = Process::Start(grandchildHoldsThePipe());
    killDirectChildAndSettle(process);

    ProcessStartInfo second("/bin/sh");
    second.getArgumentListProperty().push_back("-c");
    second.getArgumentListProperty().push_back("exec true");
    process.setStartInfoProperty(second);

    const auto started = std::chrono::steady_clock::now();
    ASSERT_TRUE(process.Start()) << "the restart itself must keep working";
    const long blockedMs = elapsedMsSince(started);

    EXPECT_GE(blockedMs, kBlockedFloorMs)
        << "the restart returned promptly -- the reader-join policy (#2029) appears to have "
           "landed; retire this pin";

    process.WaitForExit();
}

// The control that makes the four pins above mean what they say: with the SAME shell, the same
// redirection and the same timings, a child that `exec`s instead of forking leaves no descendant
// holding the pipe, and every one of those calls returns promptly. So the pins detect the
// inherited-pipe holder specifically, not "Process calls are slow".
TEST(ProcessReaderJoinBlockingPinTests, ControlNoGrandchildMeansNoBlocking) {
    ProcessStartInfo startInfo("/bin/sh");
    startInfo.getArgumentListProperty().push_back("-c");
    startInfo.getArgumentListProperty().push_back("exec sleep " + std::to_string(kKeepAliveSeconds));
    startInfo.setRedirectStandardOutputProperty(true);

    Process process = Process::Start(startInfo);
    killDirectChildAndSettle(process);

    const auto started = std::chrono::steady_clock::now();
    EXPECT_TRUE(process.getHasExitedProperty());
    process.Kill();
    const long blockedMs = elapsedMsSince(started);

    EXPECT_LT(blockedMs, kBlockedFloorMs)
        << "even without a pipe-holding descendant these calls blocked -- the pins above no "
           "longer isolate the grandchild";
}

TEST(ProcessReaderJoinBlockingPinTests, ZZZ_NoZombieChildrenRemain) {
    int status = 0;
    const pid_t reaped = ::waitpid(-1, &status, WNOHANG);
    EXPECT_EQ(reaped, -1) << "an unreaped child process survived this suite";
    EXPECT_EQ(errno, ECHILD);
}
