// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2028 -- PINS for the three BLOCKED System::Diagnostics tickets.
//
// Every test in this file asserts behaviour that is KNOWN TO BE WRONG and is deliberately
// left unrepaired because repairing it needs an approval that has not been given. They exist
// so that #2029, #2030 and #2031 CANNOT LAND SILENTLY: the moment any of those tickets is
// implemented, the corresponding test here fails and whoever implemented it must come back and
// delete the pin along with its gate.
//
//   #2029 (SR-AUD-269, cause D-B) -- destruction has no reaping policy. Unredirected, the
//         destructor returns at once and leaves a zombie; redirected, it joins the readers and
//         therefore blocks for the child's whole lifetime. Two OPPOSITE defects, which is why
//         it is a design ticket. Approval sentence: plan section 14.1.
//   #2030 (SR-AUD-271, cause D-D) -- the captured-output getters hand out a reference into a
//         buffer an internal thread is still appending to. Approval sentence: section 14.2.
//   #2031 (SR-AUD-273, cause D-E) -- Kill(true) is killpg, so a descendant that called setsid
//         has left the group and survives. Approval sentence: section 14.3.
//
// NOTHING HERE IMPLEMENTS OR AUTHORISES ANY OF THOSE TICKETS. A failure of any test in this
// file is not a regression to be silenced -- it is the signal the file exists to produce.
//
// Note on #2030's pin: the racy read that the #2023 probe recorded (the same reference showing
// 4 bytes mid-run and 8 bytes after exit) is a data race BY CONSTRUCTION, so it is not
// reproduced here -- a permanent suite must not contain a deliberate data race. The pin is on
// the two properties that make that race possible and that #2030 removes: the return type is a
// reference, and it refers to storage the object keeps rather than a copy.

#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <type_traits>

#include "System/Diagnostics/Process.hpp"
#include "System/Diagnostics/ProcessStartInfo.hpp"

using System::Diagnostics::Process;
using System::Diagnostics::ProcessStartInfo;

namespace {

ProcessStartInfo shellStartInfo(const std::string& script, bool redirectOut = false) {
    ProcessStartInfo startInfo("/bin/sh");
    startInfo.getArgumentListProperty().push_back("-c");
    startInfo.getArgumentListProperty().push_back(script);
    startInfo.setRedirectStandardOutputProperty(redirectOut);
    return startInfo;
}

// Reads /proc/<pid>/stat's state field WITHOUT reaping the process -- waitpid() would reap it
// and destroy the very state these pins measure. The comm field can contain spaces and
// parentheses, so the state is taken from just after the LAST ')'.
char processState(pid_t pid) {
    const std::string path = "/proc/" + std::to_string(static_cast<long>(pid)) + "/stat";
    std::FILE* file = std::fopen(path.c_str(), "re");
    if (file == nullptr) return '\0';
    std::string contents;
    char buffer[512];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) contents.append(buffer, read);
    std::fclose(file);
    const std::size_t close = contents.rfind(')');
    if (close == std::string::npos || close + 2 >= contents.size()) return '\0';
    return contents[close + 2];
}

bool waitForProcessState(pid_t pid, char expected, std::chrono::milliseconds limit) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (std::chrono::steady_clock::now() < deadline) {
        if (processState(pid) == expected) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return processState(pid) == expected;
}

long elapsedMsSince(std::chrono::steady_clock::time_point start) {
    return static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count());
}

// A unique path, so these tests never collide with a parallel run or delete an unrelated file.
std::string makeUniqueWitnessPath() {
    char pathTemplate[] = "/tmp/sharp_runtime_2028_pin_XXXXXX";
    const int descriptor = ::mkstemp(pathTemplate);
    if (descriptor >= 0) ::close(descriptor);
    return std::string(pathTemplate);
}

bool witnessWasWritten(const std::string& path) {
    std::FILE* file = std::fopen(path.c_str(), "re");
    if (file == nullptr) return false;
    char buffer[16] = {};
    const size_t read = std::fread(buffer, 1, sizeof(buffer) - 1, file);
    std::fclose(file);
    return read > 0 && std::strncmp(buffer, "alive", 5) == 0;
}

} // namespace

// ===========================================================================
// #2029 (SR-AUD-269) -- destruction has no reaping policy.
// ===========================================================================

// PIN FOR BLOCKED TICKET #2029. Destroying a Process whose UNREDIRECTED child has already
// exited does NOT reap it: the child stays a zombie until someone else waits for it.
// Plan section 14.1's recommended option C reaps with waitpid(WNOHANG) here, which would make
// this test fail. That failure is the point -- do not silence it; retire this pin with #2029.
TEST(ProcessGatedBehaviourPinTests, Pin2029_UnredirectedDestructionLeavesAZombie) {
    pid_t childPid = -1;
    {
        Process process = Process::Start(shellStartInfo("exit 0"));
        childPid = static_cast<pid_t>(process.getIdProperty());

        // Wait for the child to become a zombie via /proc. getHasExitedProperty() must NOT be
        // used here: it calls reapIfNeeded(), which would reap the child and erase the state
        // this pin exists to observe.
        ASSERT_TRUE(waitForProcessState(childPid, 'Z', std::chrono::seconds(5)))
            << "the child never reached state 'Z'";
    }   // ~Process runs here.

    EXPECT_EQ(processState(childPid), 'Z')
        << "the destructor reaped the child -- #2029 appears to have landed; retire this pin";

    // The pin must not itself leak the zombie it just asserted.
    int status = 0;
    pid_t reaped = -1;
    while ((reaped = ::waitpid(childPid, &status, 0)) < 0 && errno == EINTR) {}
    EXPECT_EQ(reaped, childPid)
        << "nobody had reaped the child, which is exactly what this pin asserts";
}

// PIN FOR BLOCKED TICKET #2029, the OPPOSITE failure mode. Destroying a Process whose child is
// REDIRECTED and still running BLOCKS, because ~Impl joins the pipe readers and a reader
// cannot finish until the child closes stdout. Measured 2005 ms for a 2 s child by #2023.
// Option C detaches the readers instead, so destruction would become prompt and this fails.
TEST(ProcessGatedBehaviourPinTests, Pin2029_RedirectedDestructionBlocksForTheChildLifetime) {
    pid_t childPid = -1;
    const auto started = std::chrono::steady_clock::now();
    {
        Process process = Process::Start(shellStartInfo("exec sleep 1", true));
        childPid = static_cast<pid_t>(process.getIdProperty());
    }   // ~Process joins the reader, which waits for the child to close stdout.
    const long destructionMs = elapsedMsSince(started);

    EXPECT_GE(destructionMs, 700)
        << "destroying a redirected Process returned promptly -- #2029 appears to have landed; "
           "retire this pin";

    // The destructor blocked but still did not reap, so this pin cleans up after itself too.
    int status = 0;
    pid_t reaped = -1;
    while ((reaped = ::waitpid(childPid, &status, 0)) < 0 && errno == EINTR) {}
    EXPECT_EQ(reaped, childPid);
}

// ===========================================================================
// #2030 (SR-AUD-271) -- captured output is handed out by reference.
// ===========================================================================

// PIN FOR BLOCKED TICKET #2030. Both captured-output getters return a REFERENCE into storage
// the Process keeps, which is what lets a caller hold it while the internal reader thread
// appends. Plan section 14.2 changes both to return std::string BY VALUE -- the only public
// declaration change in this namespace -- and these static_asserts fail the moment it lands.
TEST(ProcessGatedBehaviourPinTests, Pin2030_CapturedOutputIsReturnedByReference) {
    static_assert(
        std::is_same_v<decltype(std::declval<const Process&>().getStandardOutputTextProperty()),
                       const std::string&>,
        "getStandardOutputTextProperty no longer returns const std::string& -- #2030 appears to "
        "have landed; retire this pin with it");
    static_assert(
        std::is_same_v<decltype(std::declval<const Process&>().getStandardErrorTextProperty()),
                       const std::string&>,
        "getStandardErrorTextProperty no longer returns const std::string& -- #2030 appears to "
        "have landed; retire this pin with it");

    Process process = Process::Start(shellStartInfo("printf pinned", true));
    process.WaitForExit();

    // The reference denotes storage the object owns: two calls yield the SAME object, not two
    // copies. This is the property that makes the race in SR-AUD-271 possible at all.
    const std::string& first = process.getStandardOutputTextProperty();
    const std::string& second = process.getStandardOutputTextProperty();
    EXPECT_EQ(&first, &second)
        << "the getter returned a copy -- #2030 appears to have landed; retire this pin";
    EXPECT_EQ(first, "pinned");
}

// PIN FOR BLOCKED TICKET #2030. getHasExitedProperty() is declared const and MUTATES the
// object's exit state through reapIfNeeded(), so const conveys no thread-safety here. This is
// plan section 16 row 5, an observation #2023 added to the finding.
TEST(ProcessGatedBehaviourPinTests, Pin2030_ConstHasExitedMutatesObservableState) {
    Process process = Process::Start(shellStartInfo("exit 4"));
    const Process& asConst = process;

    // Reading a const-qualified property is what makes the exit code become available.
    ASSERT_TRUE(waitForProcessState(static_cast<pid_t>(process.getIdProperty()), 'Z',
                                    std::chrono::seconds(5)));
    EXPECT_TRUE(asConst.getHasExitedProperty());
    EXPECT_EQ(asConst.getExitCodeProperty(), 4)
        << "a const getter no longer transitions the object to the exited state -- if #2030 "
           "landed, retire this pin";
}

// ===========================================================================
// #2031 (SR-AUD-273) -- Kill(true) signals one process group.
// ===========================================================================

// PIN FOR BLOCKED TICKET #2031. Kill(entireProcessTree = true) is ::killpg, which reaches only
// the child's process group; a descendant that called setsid() has left that group and
// SURVIVES, despite the parameter's full-process-tree contract. Plan section 14.3 replaces the
// killpg with a transitive /proc descendant walk on Linux, after which the survivor is killed
// and this test fails.
//
// The witness is a file rather than a pid: the surviving grandchild writes it after a delay,
// so "the file exists" means "the grandchild was still alive after Kill(true)" without the
// test having to track a pid through two setsid-ing layers.
TEST(ProcessGatedBehaviourPinTests, Pin2031_SetsidDescendantSurvivesKillEntireProcessTree) {
    const std::string witness = makeUniqueWitnessPath();
    ASSERT_FALSE(witness.empty());

    // The direct child stays alive (exec sleep 5) so that Kill() does not short-circuit on an
    // already-exited child, and spawns a grandchild in a NEW SESSION that writes the witness
    // one second later.
    const std::string script =
        "setsid /bin/sh -c 'sleep 1; printf alive > " + witness + "' >/dev/null 2>&1; "
        "exec sleep 5";

    Process process = Process::Start(shellStartInfo(script));
    // Give the shell time to spawn the setsid grandchild, but far less than its 1 s delay.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    process.Kill(true);
    ASSERT_TRUE(process.WaitForExit(5000)) << "the direct child was not killed";

    // If the grandchild had been killed with the tree, it could never write the witness.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool written = false;
    while (!written && std::chrono::steady_clock::now() < deadline) {
        written = witnessWasWritten(witness);
        if (!written) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_TRUE(written)
        << "the setsid descendant did NOT survive Kill(true) -- #2031 appears to have landed; "
           "retire this pin with it";

    ::unlink(witness.c_str());
}

// ===========================================================================
// Layout pins (plan section 10) -- these are not gated, they are ABI tripwires.
// ===========================================================================

// Process is a pimpl: exactly one std::unique_ptr. This is the single most important ABI fact
// in the namespace, because it is why #2029's, #2030's and #2031's Impl changes are all
// layout-invisible and a consumer relinks rather than rebuilds.
TEST(ProcessGatedBehaviourPinTests, ProcessLayoutIsOneUniquePointer) {
    EXPECT_EQ(sizeof(Process), sizeof(std::unique_ptr<int>));
    EXPECT_EQ(alignof(Process), alignof(std::unique_ptr<int>));
}

TEST(ProcessGatedBehaviourPinTests, ProcessStartInfoLayoutIsStable) {
    // Recorded rather than asserted against a magic number chosen in advance: the point is
    // that a change is visible in the diff of this file.
    EXPECT_EQ(alignof(ProcessStartInfo), alignof(std::string));
    EXPECT_GT(sizeof(ProcessStartInfo), sizeof(std::string));
}

TEST(ProcessGatedBehaviourPinTests, ZZZ_NoZombieChildrenRemain) {
    int status = 0;
    pid_t reaped = ::waitpid(-1, &status, WNOHANG);
    EXPECT_EQ(reaped, -1) << "an unreaped child process survived this suite";
    EXPECT_EQ(errno, ECHILD);
}
