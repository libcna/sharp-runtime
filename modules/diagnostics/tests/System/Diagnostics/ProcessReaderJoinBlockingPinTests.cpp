// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2033 -- PINS for the reader-thread join, which is reached from FIVE public doors.
//
// #2032 LANDED 2026-08-19 and inverted three of the four pins below. The file's original text
// follows, because the measurements in it are what made the repair checkable.
//
// The choice this file said had to be made -- join the reader, detach it, or abandon the
// descriptor -- turned out to be a FALSE TRICHOTOMY, and the reference says so. Reaping the CHILD
// and waiting for its OUTPUT are two different things, and .NET does the second in exactly one
// place: `WaitForExitCore` waits for `_output.EOF` only when `milliseconds == Timeout.Infinite`,
// with the comment "if we have a hard timeout, we cannot wait for the streams". So the readers are
// neither detached nor abandoned -- they are simply not waited for by doors that have a deadline
// or no stream contract, and ~Impl still joins them, bounded by #2029's stop flag.
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

// INVERTED by #2032. WaitForExit(milliseconds) honours its own declared bound.
TEST(ProcessReaderJoinBlockingPinTests, Fix2032_WaitForExitTimeoutIsHonouredBehindAGrandchild) {
    Process process = Process::Start(grandchildHoldsThePipe());
    killDirectChildAndSettle(process);

    const auto started = std::chrono::steady_clock::now();
    const bool exited = process.WaitForExit(200);
    const long blockedMs = elapsedMsSince(started);

    // The child really has exited -- the bound is honoured by not waiting for its OUTPUT, not by
    // failing to observe its exit.
    EXPECT_TRUE(exited);
    EXPECT_LT(blockedMs, kBlockedFloorMs)
        << "WaitForExit(200) blocked for " << blockedMs << " ms behind the grandchild";
}

// INVERTED by #2032, on a door #2032 did not name -- #2033 measured that the same join was
// reached from five. .NET's HasExited waits for no stream at all, so this door had no business
// blocking on one.
TEST(ProcessReaderJoinBlockingPinTests, Fix2032_HasExitedDoesNotBlockBehindAGrandchild) {
    Process process = Process::Start(grandchildHoldsThePipe());
    killDirectChildAndSettle(process);

    const auto started = std::chrono::steady_clock::now();
    const bool exited = process.getHasExitedProperty();
    const long blockedMs = elapsedMsSince(started);

    EXPECT_TRUE(exited);
    EXPECT_LT(blockedMs, kBlockedFloorMs)
        << "getHasExitedProperty() blocked for " << blockedMs << " ms";
}

// INVERTED by #2032, second unnamed door. Kill() is documented as IMMEDIATELY stopping the
// process, and .NET's waits for no stream either.
TEST(ProcessReaderJoinBlockingPinTests, Fix2032_KillDoesNotBlockBehindAGrandchild) {
    Process process = Process::Start(grandchildHoldsThePipe());
    killDirectChildAndSettle(process);

    const auto started = std::chrono::steady_clock::now();
    process.Kill();
    const long blockedMs = elapsedMsSince(started);

    EXPECT_LT(blockedMs, kBlockedFloorMs) << "Kill() blocked for " << blockedMs << " ms";
}

// THE TIMEOUT-EXPIRY PATH, which the case above does NOT reach: when the child has already
// exited, WaitForExit(ms) returns from its early `if (hasExited) return true`. The fall-through
// after the deadline is a different statement, and a mutation that joins the readers THERE went
// uncaught until this case existed. Here the child is still running, so the call must time out
// and return false without waiting for anything.
TEST(ProcessReaderJoinBlockingPinTests, Fix2032_AnExpiredTimeoutReturnsWithoutJoiningEither) {
    Process process = Process::Start(grandchildHoldsThePipe());

    const auto started = std::chrono::steady_clock::now();
    const bool exited = process.WaitForExit(200);
    const long blockedMs = elapsedMsSince(started);

    EXPECT_FALSE(exited) << "the child should still be running";
    EXPECT_LT(blockedMs, kBlockedFloorMs)
        << "an expired WaitForExit(200) blocked for " << blockedMs << " ms";

    process.Kill();
    process.WaitForExit();
}

// THE OTHER HALF, and the reason this is a repair rather than a removal: the ONE door that has no
// deadline still waits for the output, so a caller who wants the complete output can still get it.
// That is .NET's rule exactly -- `WaitForExitCore` waits for `_output.EOF` only when
// `milliseconds == Timeout.Infinite`, with the comment "if we have a hard timeout, we cannot wait
// for the streams".
TEST(ProcessReaderJoinBlockingPinTests, Fix2032_TheUnboundedOverloadStillWaitsForTheOutput) {
    // HONEST NOTE ON WHAT THIS CASE DOES AND DOES NOT CATCH. It states the contract -- the
    // unbounded overload waits for the output -- and it does NOT discriminate a mutation that
    // removes that join: measured, the reader drains 16 KiB well before waitpid() reports the
    // exit, so the output is complete either way. The mutation IS caught, by seven other tests
    // including five `ZZZ_NoZombieChildrenRemain` checks and
    // ProcessForkSafetyTests.StartsWithEnvironmentWhileReaderThreadsAreLive, which is the
    // evidence that the join is load-bearing. This case is kept because it is the only place the
    // CONTRACT is written down, not because it is the mutation's detector.
    ProcessStartInfo startInfo("/bin/sh");
    startInfo.getArgumentListProperty().push_back("-c");
    startInfo.getArgumentListProperty().push_back(
        "i=0; while [ $i -lt 400 ]; do printf '0123456789012345678901234567890123456789'; "
        "i=$((i+1)); done");
    startInfo.setRedirectStandardOutputProperty(true);

    Process process = Process::Start(startInfo);
    process.WaitForExit();
    // Complete, because the unbounded overload joined the reader.
    EXPECT_EQ(process.getStandardOutputTextProperty().size(), 400u * 40u);
}

// STILL PINNED, and deliberately: the restart is the one door whose join #2032 does NOT remove.
// #2025's restart preamble resolves the previous child's readers before assigning over the
// std::thread, and in C++ assigning to a JOINABLE std::thread calls std::terminate -- so this join
// is structurally required by the language, not by a policy choice. .NET has no counterpart: its
// Process.Start on a live object does not reuse the reader machinery this port must.
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
