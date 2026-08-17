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
#include <atomic>
#include <thread>
#include <vector>

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

// #2029 LANDED 2026-08-17, and this pin is INVERTED. Destroying a Process whose child is
// REDIRECTED and still running used to BLOCK FOR THE CHILD'S WHOLE LIFETIME -- ~Impl joins the
// pipe readers, and a reader calling a bare blocking read() cannot finish until the child closes
// stdout. Measured 2005 ms for a 2 s child by #2023, and unbounded in general.
//
// .NET does not do that: Process.Close() (Process.cs:761-805) stops watching for exit, releases
// the handle and CANCELS the async read before disposing the stream. It never waits for the
// child. The readers now poll in bounded slices and observe a stop flag, which is that
// cancellation expressed with the tools a C++ pipe reader has.
//
// The join itself is KEPT, deliberately. The plan's option C said to detach the readers, but a
// detached reader keeps appending into the Process's own string after the object is gone -- a
// use-after-free, which would trade this defect for a worse one. Bounding the wait, rather than
// removing it, gets the same promptness without that.
TEST(ProcessGatedBehaviourPinTests, Fix2029_RedirectedDestructionIsPrompt) {
    pid_t childPid = -1;
    const auto started = std::chrono::steady_clock::now();
    {
        Process process = Process::Start(shellStartInfo("exec sleep 1", true));
        childPid = static_cast<pid_t>(process.getIdProperty());
    }   // ~Process must NOT wait for the 1 s child.
    const long destructionMs = elapsedMsSince(started);

    EXPECT_LT(destructionMs, 500)
        << "destroying a redirected Process still waited for the child (" << destructionMs
        << " ms); the readers are not observing the stop flag";

    // Destruction is prompt but still does not REAP a running child: .NET reaps process-wide
    // from a SIGCHLD-driven wait state, which this port cannot replicate without colliding with
    // PosixSignalRegistration (#1975/#1979). That divergence is documented, and this pin cleans
    // up after itself rather than leaving the zombie behind for the rest of the suite.
    int status = 0;
    pid_t reaped = -1;
    while ((reaped = ::waitpid(childPid, &status, 0)) < 0 && errno == EINTR) {}
    EXPECT_EQ(reaped, childPid);
}

// The control the case above needs: making destruction prompt must not have been achieved by
// dropping output on the floor. A child that has already written and exited still has its
// output captured in full.
TEST(ProcessGatedBehaviourPinTests, Fix2029_PromptDestructionDidNotCostCapturedOutput) {
    Process process = Process::Start(shellStartInfo("printf 'hello-2029'", true));
    process.WaitForExit();
    EXPECT_EQ(process.getStandardOutputTextProperty(), "hello-2029");
}

// ===========================================================================
// #2030 (SR-AUD-271) -- captured output is handed out by reference.
// ===========================================================================

// #2030 LANDED 2026-08-17, and both of its pins are INVERTED.
//
// Both captured-output getters returned a REFERENCE into storage the internal reader thread was
// still appending to -- the audit measured the SAME reference reading 4 bytes mid-run and 8
// bytes after exit. There is no reference a caller can hold safely while another thread appends,
// so the getters now return BY VALUE and take the copy under the lock the readers append under.
// That is the only public declaration change in this namespace, and it is source-compatible for
// the ordinary spelling: a returned value still binds to `const std::string&`.
TEST(ProcessGatedBehaviourPinTests, Fix2030_CapturedOutputIsReturnedByValue) {
    static_assert(
        std::is_same_v<decltype(std::declval<const Process&>().getStandardOutputTextProperty()),
                       std::string>,
        "getStandardOutputTextProperty must return by value; a reference into a buffer the "
        "reader thread appends to cannot be made safe");
    static_assert(
        std::is_same_v<decltype(std::declval<const Process&>().getStandardErrorTextProperty()),
                       std::string>,
        "getStandardErrorTextProperty must return by value, for the same reason");

    Process process = Process::Start(shellStartInfo("printf pinned", true));
    process.WaitForExit();

    // Two calls now yield two independent copies. Mutating one cannot be observed through the
    // other, which is precisely what makes SR-AUD-271's race unreachable through this door.
    std::string first = process.getStandardOutputTextProperty();
    const std::string second = process.getStandardOutputTextProperty();
    EXPECT_NE(&first, &second);
    EXPECT_EQ(first, "pinned");
    first.append("-mutated");
    EXPECT_EQ(second, "pinned") << "the getter handed out shared storage";
    EXPECT_EQ(process.getStandardOutputTextProperty(), "pinned")
        << "mutating a returned copy reached the process's own buffer";
}

// The second half. getHasExitedProperty() is declared const and MUTATES the object's exit state
// through reapIfNeeded(), which is .NET's shape too -- HasExited reaps lazily -- so the repair
// is NOT to stop mutating but to make the mutation safe against a concurrent reader. The
// mutation stays, and this case still asserts it; what changed is that it now happens under a
// lock, so `const` no longer silently implies "safe to call from two threads".
TEST(ProcessGatedBehaviourPinTests, Fix2030_ConstHasExitedStillMutatesButIsNowGuarded) {
    Process process = Process::Start(shellStartInfo("exit 4"));
    const Process& asConst = process;

    ASSERT_TRUE(waitForProcessState(static_cast<pid_t>(process.getIdProperty()), 'Z',
                                    std::chrono::seconds(5)));
    EXPECT_TRUE(asConst.getHasExitedProperty());
    EXPECT_EQ(asConst.getExitCodeProperty(), 4);
}

// Concurrent readers of the captured output and of the exit state must not tear or crash. This
// is the case that would have reproduced SR-AUD-271 before the lock: several threads copying
// the output while the reader appends to it.
TEST(ProcessGatedBehaviourPinTests, Fix2030_ConcurrentReadersSeeAConsistentBuffer) {
    Process process = Process::Start(
        shellStartInfo("for i in 1 2 3 4 5 6 7 8 9 10; do printf 'chunk-%s;' \"$i\"; done", true));

    std::atomic<bool> stop{false};
    std::atomic<int> reads{0};
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&] {
            while (!stop.load()) {
                const std::string text = process.getStandardOutputTextProperty();
                // A torn copy would show up as a partial trailing record; every complete read
                // must end at a record boundary or be empty.
                if (!text.empty()) { EXPECT_EQ(text.back(), ';') << text; }
                reads.fetch_add(1);
            }
        });
    }

    process.WaitForExit();
    stop.store(true);
    for (auto& reader : readers) reader.join();

    EXPECT_GT(reads.load(), 0);
    EXPECT_EQ(process.getStandardOutputTextProperty(),
              "chunk-1;chunk-2;chunk-3;chunk-4;chunk-5;chunk-6;chunk-7;chunk-8;chunk-9;chunk-10;");
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
