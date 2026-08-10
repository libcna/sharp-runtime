// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2026 (SR-AUD-274, cause D-F of docs/SystemDiagnosticsNamespaceReviewPlan.md).
//
// Between fork() and exec() a child inherits the whole parent address space but only the
// forking thread, so it may call only async-signal-safe functions. The child used to call
// ::setenv once per configured variable; ::setenv allocates, so a child forked while another
// thread held the malloc lock could deadlock there and never exec -- and the parent would then
// block forever in Start's status-pipe read. The parent is multithreaded exactly when a
// previous redirected Start left this class's own pipe-reader threads running, so the hazard
// needs no caller-created thread to be reachable.
//
// The environment is now marshalled in the PARENT and handed to execvpe (or, where that is
// absent, installed by replacing the global environ pointer -- a single store). These tests
// pin the OBSERVABLE contract that rewrite had to preserve exactly: the child's environment is
// still the parent's, with the configured variables overriding same-named entries. The
// deadlock itself is a race and cannot be asserted directly; what is asserted is that the
// shape which made it reachable is exercised repeatedly without hanging.

#include <gtest/gtest.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <string>

#include "System/ArgumentException.hpp"
#include "System/Diagnostics/Process.hpp"
#include "System/InvalidOperationException.hpp"

using System::Diagnostics::Process;
using System::Diagnostics::ProcessStartInfo;

namespace {

constexpr const char* InheritedKey = "SHARP_RUNTIME_FORK_SAFETY_INHERITED_2026";
constexpr const char* OverriddenKey = "SHARP_RUNTIME_FORK_SAFETY_OVERRIDDEN_2026";

ProcessStartInfo shellStartInfo(const std::string& script, bool redirectOut, bool redirectErr) {
    ProcessStartInfo startInfo("/bin/sh");
    startInfo.getArgumentListProperty().push_back("-c");
    startInfo.getArgumentListProperty().push_back(script);
    startInfo.setRedirectStandardOutputProperty(redirectOut);
    startInfo.setRedirectStandardErrorProperty(redirectErr);
    return startInfo;
}

std::string captureShellOutput(ProcessStartInfo startInfo) {
    startInfo.setRedirectStandardOutputProperty(true);
    Process process = Process::Start(startInfo);
    process.WaitForExit();
    return process.getStandardOutputTextProperty();
}

} // namespace

// ---------------------------------------------------------------------------
// The reachable multithreaded-parent shape: start while this class's own reader
// threads are live, repeatedly. Before #2026 every one of these forks carried a
// child that would allocate before exec.
// ---------------------------------------------------------------------------

TEST(ProcessForkSafetyTests, StartsRepeatedlyWhileTwoReaderThreadsAreLive) {
    Process keepAlive = Process::Start(shellStartInfo("exec sleep 30", true, true));

    for (int iteration = 0; iteration < 30; ++iteration) {
        Process child = Process::Start(shellStartInfo("exit 0", false, false));
        child.WaitForExit();
        ASSERT_EQ(child.getExitCodeProperty(), 0) << "iteration " << iteration;
    }

    keepAlive.Kill();
    ASSERT_TRUE(keepAlive.WaitForExit(5000));
}

TEST(ProcessForkSafetyTests, StartsWithEnvironmentWhileReaderThreadsAreLive) {
    Process keepAlive = Process::Start(shellStartInfo("exec sleep 30", true, true));

    for (int iteration = 0; iteration < 15; ++iteration) {
        ProcessStartInfo startInfo =
            shellStartInfo("printf '%s' \"$SHARP_RUNTIME_FORK_SAFETY_OVERRIDDEN_2026\"", true, false);
        startInfo.getEnvironmentVariablesProperty()[OverriddenKey] = std::to_string(iteration);
        Process child = Process::Start(startInfo);
        child.WaitForExit();
        ASSERT_EQ(child.getStandardOutputTextProperty(), std::to_string(iteration))
            << "iteration " << iteration;
    }

    keepAlive.Kill();
    ASSERT_TRUE(keepAlive.WaitForExit(5000));
}

// ---------------------------------------------------------------------------
// The environment the parent now builds must be exactly what setenv produced.
// ---------------------------------------------------------------------------

TEST(ProcessForkSafetyTests, ChildInheritsParentVariablesThatAreNotOverridden) {
    ASSERT_EQ(::setenv(InheritedKey, "from-parent", 1), 0);

    const std::string captured = captureShellOutput(
        shellStartInfo("printf '%s' \"$SHARP_RUNTIME_FORK_SAFETY_INHERITED_2026\"", true, false));

    EXPECT_EQ(captured, "from-parent");
    ::unsetenv(InheritedKey);
}

TEST(ProcessForkSafetyTests, OverrideReplacesAnExistingParentVariableExactlyOnce) {
    ASSERT_EQ(::setenv(OverriddenKey, "from-parent", 1), 0);

    ProcessStartInfo startInfo = shellStartInfo(
        "printf '%s' \"$SHARP_RUNTIME_FORK_SAFETY_OVERRIDDEN_2026\"", true, false);
    startInfo.getEnvironmentVariablesProperty()[OverriddenKey] = "from-child";

    EXPECT_EQ(captureShellOutput(startInfo), "from-child");

    // The parent's own environment must be untouched by the marshalling.
    const char* parentValue = ::getenv(OverriddenKey);
    ASSERT_NE(parentValue, nullptr);
    EXPECT_EQ(std::string(parentValue), "from-parent");
    ::unsetenv(OverriddenKey);
}

TEST(ProcessForkSafetyTests, OverrideAppearsExactlyOnceInTheChildEnvironment) {
    ASSERT_EQ(::setenv(OverriddenKey, "from-parent", 1), 0);

    ProcessStartInfo startInfo = shellStartInfo(
        "env | grep -c '^SHARP_RUNTIME_FORK_SAFETY_OVERRIDDEN_2026='", true, false);
    startInfo.getEnvironmentVariablesProperty()[OverriddenKey] = "from-child";

    EXPECT_EQ(captureShellOutput(startInfo), "1\n");
    ::unsetenv(OverriddenKey);
}

TEST(ProcessForkSafetyTests, VariableValueMayContainEqualsSign) {
    ProcessStartInfo startInfo = shellStartInfo(
        "printf '%s' \"$SHARP_RUNTIME_FORK_SAFETY_OVERRIDDEN_2026\"", true, false);
    startInfo.getEnvironmentVariablesProperty()[OverriddenKey] = "a=b=c";

    EXPECT_EQ(captureShellOutput(startInfo), "a=b=c");
}

TEST(ProcessForkSafetyTests, EmptyValueIsStillPassedLiterally) {
    ProcessStartInfo startInfo =
        shellStartInfo("test -z \"$SHARP_RUNTIME_FORK_SAFETY_OVERRIDDEN_2026\" && "
                       "env | grep -qc '^SHARP_RUNTIME_FORK_SAFETY_OVERRIDDEN_2026='",
                       false, false);
    startInfo.getEnvironmentVariablesProperty()[OverriddenKey] = "";

    Process process = Process::Start(startInfo);
    process.WaitForExit();
    EXPECT_EQ(process.getExitCodeProperty(), 0);
}

TEST(ProcessForkSafetyTests, ManyEnvironmentVariablesAreAllDelivered) {
    ProcessStartInfo startInfo = shellStartInfo(
        "printf '%s,%s,%s' \"$SHARP_RUNTIME_FORK_SAFETY_V0\" "
        "\"$SHARP_RUNTIME_FORK_SAFETY_V50\" \"$SHARP_RUNTIME_FORK_SAFETY_V99\"", true, false);
    for (int index = 0; index < 100; ++index) {
        startInfo.getEnvironmentVariablesProperty()
            ["SHARP_RUNTIME_FORK_SAFETY_V" + std::to_string(index)] = "v" + std::to_string(index);
    }

    EXPECT_EQ(captureShellOutput(startInfo), "v0,v50,v99");
}

TEST(ProcessForkSafetyTests, InvalidVariableNamesAreStillRejectedBeforeForking) {
    ProcessStartInfo startInfo("/bin/true");
    startInfo.getEnvironmentVariablesProperty()["INVALID=NAME"] = "value";
    EXPECT_THROW(Process::Start(startInfo), System::ArgumentException);

    ProcessStartInfo emptyName("/bin/true");
    emptyName.getEnvironmentVariablesProperty()[""] = "value";
    EXPECT_THROW(Process::Start(emptyName), System::ArgumentException);
}

// ---------------------------------------------------------------------------
// The status pipe must still report every pre-exec failure synchronously.
// ---------------------------------------------------------------------------

TEST(ProcessForkSafetyTests, ExecFailureIsStillReportedSynchronously) {
    ProcessStartInfo startInfo("/definitely-missing-sharp-runtime-2026");
    startInfo.getEnvironmentVariablesProperty()["SHARP_RUNTIME_FORK_SAFETY_X"] = "1";
    EXPECT_THROW(Process::Start(startInfo), System::InvalidOperationException);
}

TEST(ProcessForkSafetyTests, WorkingDirectoryFailureIsStillReportedSynchronously) {
    ProcessStartInfo startInfo("/bin/true");
    startInfo.setWorkingDirectoryProperty("/definitely-missing-sharp-runtime-dir-2026");
    startInfo.getEnvironmentVariablesProperty()["SHARP_RUNTIME_FORK_SAFETY_X"] = "1";
    EXPECT_THROW(Process::Start(startInfo), System::InvalidOperationException);
}

TEST(ProcessForkSafetyTests, WorkingDirectoryIsStillHonouredAlongsideAnOverride) {
    ProcessStartInfo startInfo("/bin/sh");
    startInfo.getArgumentListProperty().push_back("-c");
    startInfo.getArgumentListProperty().push_back(
        "printf '%s:%s' \"$(pwd)\" \"$SHARP_RUNTIME_FORK_SAFETY_OVERRIDDEN_2026\"");
    startInfo.setWorkingDirectoryProperty("/tmp");
    startInfo.getEnvironmentVariablesProperty()[OverriddenKey] = "ok";

    EXPECT_EQ(captureShellOutput(startInfo), "/tmp:ok");
}

TEST(ProcessForkSafetyTests, ZZZ_NoZombieChildrenRemain) {
    int status = 0;
    pid_t reaped = ::waitpid(-1, &status, WNOHANG);
    EXPECT_EQ(reaped, -1) << "an unreaped child process survived this suite";
    EXPECT_EQ(errno, ECHILD);
}
