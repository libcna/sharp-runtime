// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Permanent regressions for the compatible tickets of the `modules/io` namespace review
// (docs/SystemIONamespaceReviewPlan.md, owning ticket #2097).
//
//   #2101 / SR-AUD-347, cause I-C — a raw std::filesystem_error escaped FileInfo("") and
//          DirectoryInfo(""), a std:: exception crossing a System-shaped public API.
//   #2103 / SR-AUD-345, cause I-F — FileInfo::Delete DELETED a directory while its sibling
//          File::Delete threw on the same input. The only finding in this namespace that
//          destroys user data.
//   #2099 / SR-AUD-342, cause I-A — six FileStream members ignored Close(): Length re-stat'd the
//          path, Position returned the sentinel -1, Position=, Seek and Flush all succeeded, and
//          two members checked their ARGUMENT before the closed state.
//   #2344 / SR-AUD-339, cause I-E, split from #2102 — FileSystemWatcher::setPathProperty stored
//          the new directory and did nothing else, so the old inotify watch stayed armed while
//          its events were reported with a FullPath built from the NEW directory. Also a data
//          race on directory_ between the caller and the watcher thread (TSan: 3 before, 0 after).
//   #2100 / SR-AUD-340, cause I-B — RandomAccess::Write accepted a negative count SILENTLY, and
//          every other rejection in the class was untyped: a bare IOException that named neither
//          the offending parameter nor the native reason. GetLength returned the sentinel -1.
//
//   #2098 / SR-AUD-337 + SR-AUD-343 — the four text wrappers ignored Close() entirely, so a
//          closed reader kept reading and a closed writer kept writing.
//
// Reference tree absent WHEN THE TICKETS ABOVE #2098 WERE WRITTEN: every exception type and
// paramName asserted by those sections is recorded as this port's choice in the plan, not as a
// verified match to .NET. The #2098 section is different — the reference tree is available
// again (docs/StandingApprovals.md §5.1) and its assertions are transcribed from it.
#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include <sstream>

#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/BinaryData.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/IO/Directory.hpp"
#include "System/IO/DirectoryInfo.hpp"
#include "System/IO/File.hpp"
#include "System/IO/FileInfo.hpp"
#include "System/IO/FileNotFoundException.hpp"
#include "System/IO/FileStream.hpp"
#include "System/IO/FileSystemWatcher.hpp"
#include "System/IO/IOException.hpp"
#include "System/IO/RandomAccess.hpp"
#include "System/IO/StreamReader.hpp"
#include "System/IO/StreamWriter.hpp"
#include "System/IO/StringReader.hpp"
#include "System/IO/StringWriter.hpp"
#include "System/IO/TextReader.hpp"
#include "System/IO/TextWriter.hpp"
#include "System/IO/UnmanagedMemoryStream.hpp"

using namespace System::IO;

namespace {

/// A unique directory under the repository-local build tree — never /tmp, per the build-resource
/// policy, and never a fixed path that two runs could collide on.
class IoReviewFixture : public ::testing::Test {
protected:
    std::filesystem::path root;

    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        root = std::filesystem::path("build-tmp") / "io_review" /
               (std::string(info->test_suite_name()) + "_" + info->name());
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
    [[nodiscard]] std::string under(const std::string& leaf) const {
        return (root / leaf).string();
    }
};

/// #2100's fixture: a real descriptor over a real file, plus direct /proc/self/fd accounting.
/// Plan §14 is explicit that LSan must NEVER be substituted here — it tracks memory, and the
/// characteristic resource of this module is a descriptor.
class RandomAccessFixture : public IoReviewFixture {
protected:
    int fd = -1;
    std::string path;

    void SetUp() override {
        IoReviewFixture::SetUp();
        path = under("randomaccess.bin");
        fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        ASSERT_GE(fd, 0) << "the fixture could not open its own file";
        const SharpRuntime::bytecs seed[4] = {1, 2, 3, 4};
        RandomAccess::Write(fd, seed, 4, 0);
    }
    void TearDown() override {
        if (fd >= 0) ::close(fd);
        IoReviewFixture::TearDown();
    }

    /// The number of descriptors this process currently holds.
    static int fdCount() {
        int n = 0;
        std::error_code ec;
        for (auto& entry : std::filesystem::directory_iterator("/proc/self/fd", ec)) {
            (void)entry;
            ++n;
        }
        return n;
    }
};

/// Assert that `call` throws `E` and that the exception names `expectedParam`. A bare
/// EXPECT_THROW would pass for an exception naming the wrong parameter, which is exactly the
/// defect #2100 repairs — the pre-#2100 IOException named nothing at all.
template <typename E, typename F>
void ExpectThrowsNaming(F&& call, const char* expectedParam, const char* what) {
    try {
        call();
        ADD_FAILURE() << what << ": expected a throw, but the call returned";
    } catch (const E& e) {
        EXPECT_EQ(e.getParamNameProperty(), expectedParam) << what << ": " << e.what();
    } catch (const std::exception& e) {
        ADD_FAILURE() << what << ": wrong exception type: " << e.what();
    }
}

} // namespace

// ===========================================================================
// #2101 / SR-AUD-347 — no std:: exception may escape a System-shaped API
// ===========================================================================

TEST_F(IoReviewFixture, EmptyPathFileInfoThrowsASystemException) {
    // Before #2101 this threw std::filesystem::filesystem_error("cannot make absolute path"),
    // which a caller writing `catch (const System::Exception&)` never saw.
    EXPECT_THROW(FileInfo(""), System::ArgumentException);
}

TEST_F(IoReviewFixture, EmptyPathDirectoryInfoThrowsASystemException) {
    EXPECT_THROW(DirectoryInfo(""), System::ArgumentException);
}

TEST_F(IoReviewFixture, TheEmptyPathRejectionNamesItsParameter) {
    try {
        FileInfo fi("");
        FAIL() << "an empty path must be rejected";
    } catch (const System::ArgumentException& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("path"), std::string::npos) << message;
        EXPECT_NE(message.find("empty"), std::string::npos) << message;
    }
}

TEST_F(IoReviewFixture, NoStdExceptionEscapesAnEmptyPathConstruction) {
    // The load-bearing assertion: it is not enough that *some* exception is thrown — it must
    // not be a std:: one. Catching System::Exception first and std::exception second
    // distinguishes them.
    for (int which = 0; which < 2; ++which) {
        bool systemException = false, stdException = false;
        try {
            if (which == 0) { FileInfo fi(""); (void)fi.getExistsProperty(); }
            else            { DirectoryInfo di(""); (void)di.getExistsProperty(); }
        } catch (const System::Exception&) {
            systemException = true;
        } catch (const std::exception&) {
            stdException = true;
        }
        EXPECT_TRUE(systemException) << "which=" << which;
        EXPECT_FALSE(stdException) << "a std:: exception must not cross this API; which=" << which;
    }
}

TEST_F(IoReviewFixture, AValidPathIsCompletelyUnaffected) {
    const std::string file = under("ok.txt");
    File::WriteAllText(file, "hello");
    FileInfo fi(file);
    EXPECT_TRUE(fi.getExistsProperty());
    EXPECT_EQ(fi.getLengthProperty(), 5);
    DirectoryInfo di(root.string());
    EXPECT_TRUE(di.getExistsProperty());
    // A relative path still resolves, and ToString() still returns the ORIGINAL text rather
    // than the resolved one — the pre-existing contract, unchanged by #2101.
    FileInfo relative("build-tmp");
    EXPECT_EQ(relative.ToString(), "build-tmp");
}

TEST_F(IoReviewFixture, AWhitespaceOnlyPathIsStillAcceptedDeliberately) {
    // PIN of a deliberate non-narrowing. A whitespace-only path resolves cleanly on POSIX and
    // no repository evidence says .NET rejects it at this door, so #2101 does NOT narrow it —
    // narrowing on a guess is what the review's deferred-verification rule exists to prevent.
    // If this ever starts throwing, that was a decision and it must be recorded.
    EXPECT_NO_THROW({ FileInfo fi("   "); (void)fi.getExistsProperty(); });
}

TEST_F(IoReviewFixture, TheAlreadyGuardedNeighbouringDoorsStayGuarded) {
    // Measured during the review (build-probe/2101_probe2_before.log): the sites a grep for
    // throwing std::filesystem calls flagged — DirectoryInfo's enumeration and the Copy/Move
    // destination handling — were ALREADY guarded and threw System exceptions. #2101's real
    // scope was one door, not seven. These assertions keep that true.
    DirectoryInfo missing(under("no_such_dir"));
    EXPECT_THROW((void)missing.GetFiles(), System::Exception);
    EXPECT_THROW((void)missing.GetDirectories(), System::Exception);

    const std::string file = under("src.txt");
    File::WriteAllText(file, "x");
    FileInfo fi(file);
    EXPECT_THROW((void)fi.CopyTo(""), System::Exception);
}

// ===========================================================================
// #2103 / SR-AUD-345 — FileInfo::Delete must not delete a directory
// ===========================================================================

TEST_F(IoReviewFixture, FileInfoDeleteOverAnEmptyDirectoryThrowsAndTheDirectorySurvives) {
    // Before #2103 this SILENTLY DELETED the directory: FileInfo::Delete called
    // std::filesystem::remove, which happily removes an empty directory. The surviving
    // directory is the whole point of the assertion — a throw alone would not prove the data
    // was not destroyed.
    const std::string dir = under("adir");
    std::filesystem::create_directories(dir);
    FileInfo fi(dir);
    EXPECT_THROW(fi.Delete(), System::Exception);
    EXPECT_TRUE(std::filesystem::exists(dir)) << "the directory must still exist";
    EXPECT_TRUE(std::filesystem::is_directory(dir));
}

TEST_F(IoReviewFixture, FileInfoDeleteOverANonEmptyDirectoryThrowsAndItsContentsSurvive) {
    const std::string dir = under("full");
    std::filesystem::create_directories(dir);
    File::WriteAllText(dir + "/inner.txt", "keep me");
    FileInfo fi(dir);
    EXPECT_THROW(fi.Delete(), System::Exception);
    EXPECT_TRUE(std::filesystem::exists(dir));
    EXPECT_TRUE(std::filesystem::exists(dir + "/inner.txt"));
    EXPECT_EQ(File::ReadAllText(dir + "/inner.txt"), "keep me");
}

TEST_F(IoReviewFixture, FileInfoDeleteOverARealFileStillWorks) {
    const std::string file = under("gone.txt");
    File::WriteAllText(file, "bye");
    FileInfo fi(file);
    EXPECT_NO_THROW(fi.Delete());
    EXPECT_FALSE(std::filesystem::exists(file));
}

TEST_F(IoReviewFixture, FileInfoDeleteAndFileDeleteNowAgreeOnEveryInput) {
    // The sharper framing the review found (plan §6.4): the finding names only FileInfo, but
    // the defect is that two APIs .NET documents as equivalent gave OPPOSITE answers. File::Delete
    // was already correct, so it is the reference — the repair target already existed in the
    // module rather than needing to be invented.
    const std::string dir = under("agree_dir");
    std::filesystem::create_directories(dir);

    bool fileDeleteThrew = false, fileInfoDeleteThrew = false;
    try { File::Delete(dir); } catch (const System::Exception&) { fileDeleteThrew = true; }
    try { FileInfo(dir).Delete(); } catch (const System::Exception&) { fileInfoDeleteThrew = true; }

    EXPECT_TRUE(fileDeleteThrew) << "File::Delete was already correct";
    EXPECT_EQ(fileInfoDeleteThrew, fileDeleteThrew) << "the two siblings must agree";
    EXPECT_TRUE(std::filesystem::exists(dir)) << "neither may have deleted it";
}

TEST_F(IoReviewFixture, DirectoryInfoDeleteStillDeletesADirectory) {
    // The repair must not leak onto the type whose job this actually is.
    const std::string dir = under("di_dir");
    std::filesystem::create_directories(dir);
    DirectoryInfo di(dir);
    EXPECT_NO_THROW(di.Delete());
    EXPECT_FALSE(std::filesystem::exists(dir));
}

// ===========================================================================
// #2100 / SR-AUD-340 — RandomAccess argument domain, exception identity, GetLength
// ===========================================================================

TEST_F(RandomAccessFixture, WriteWithANegativeCountNoLongerSucceedsSilently) {
    // THE SHARP HALF (plan §6.3). `while (count > 0)` fell straight through for a negative
    // count, so this SUCCEEDED: no bytes written, no error, and a void return giving the caller
    // nothing to inspect.
    SharpRuntime::bytecs buf[4] = {7, 7, 7, 7};
    ExpectThrowsNaming<System::ArgumentOutOfRangeException>(
        [&] { RandomAccess::Write(fd, buf, -1, 0); }, "count", "Write(count=-1)");
}

TEST_F(RandomAccessFixture, ARejectedWriteLeavesTheFileCompletelyUnchanged) {
    // "No partial state or output after rejection." Validation runs before the descriptor is
    // touched, so the rejected call cannot have written anything.
    RandomAccess::SetLength(fd, 0);
    const SharpRuntime::bytecs buf[4] = {7, 7, 7, 7};
    EXPECT_THROW(RandomAccess::Write(fd, buf, -1, 0), System::ArgumentOutOfRangeException);
    EXPECT_EQ(RandomAccess::GetLength(fd), 0);
    EXPECT_THROW(RandomAccess::Write(fd, buf, 4, -1), System::ArgumentOutOfRangeException);
    EXPECT_EQ(RandomAccess::GetLength(fd), 0);
}

TEST_F(RandomAccessFixture, ReadRejectsANegativeCountAndOffsetByNAMINGTHEPARAMETER) {
    // Read ALREADY threw before #2100 — its defect was the exception IDENTITY, not the
    // acceptance (plan §6.3). A bare EXPECT_THROW would have passed against the old
    // IOException("RandomAccess::Read failed"); asserting the paramName is what discriminates.
    SharpRuntime::bytecs buf[4] = {};
    ExpectThrowsNaming<System::ArgumentOutOfRangeException>(
        [&] { (void)RandomAccess::Read(fd, buf, -1, 0); }, "count", "Read(count=-1)");
    ExpectThrowsNaming<System::ArgumentOutOfRangeException>(
        [&] { (void)RandomAccess::Read(fd, buf, 2, -1); }, "fileOffset", "Read(fileOffset=-1)");
    ExpectThrowsNaming<System::ArgumentOutOfRangeException>(
        [&] { (void)RandomAccess::Read(fd, buf, 2, -5); }, "fileOffset", "Read(fileOffset=-5)");
}

TEST_F(RandomAccessFixture, TheSAMEDomainIsEnforcedOnTheSidesTheFindingNeverNames) {
    // The ticket forbids repairing only the named example. SR-AUD-340 names Write's count and
    // Read's two arguments; it names NEITHER Write's fileOffset NOR SetLength's length, and both
    // rejected untyped before #2100.
    const SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    ExpectThrowsNaming<System::ArgumentOutOfRangeException>(
        [&] { RandomAccess::Write(fd, buf, 2, -5); }, "fileOffset", "Write(fileOffset=-5)");
    ExpectThrowsNaming<System::ArgumentOutOfRangeException>(
        [&] { RandomAccess::SetLength(fd, -1); }, "length", "SetLength(length=-1)");
}

TEST_F(RandomAccessFixture, ANullBufferIsRejectedOnlyWhenBytesWouldActuallyMove) {
    // §13 requires BOTH directions. count 0 through a null pointer transfers nothing and stays
    // accepted; count > 0 through a null pointer was an EFAULT surfaced as a generic IOException.
    EXPECT_NO_THROW((void)RandomAccess::Read(fd, nullptr, 0, 0));
    EXPECT_NO_THROW(RandomAccess::Write(fd, nullptr, 0, 0));
    ExpectThrowsNaming<System::ArgumentNullException>(
        [&] { (void)RandomAccess::Read(fd, nullptr, 4, 0); }, "buffer", "Read(null, 4)");
    ExpectThrowsNaming<System::ArgumentNullException>(
        [&] { RandomAccess::Write(fd, nullptr, 4, 0); }, "buffer", "Write(null, 4)");
}

TEST_F(RandomAccessFixture, TheVectorOverloadsInheritTheSameDomainChecks) {
    std::vector<SharpRuntime::bytecs> empty;
    std::vector<SharpRuntime::bytecs> two{9, 9};
    EXPECT_NO_THROW((void)RandomAccess::Read(fd, empty, 0));
    EXPECT_NO_THROW(RandomAccess::Write(fd, empty, 0));
    ExpectThrowsNaming<System::ArgumentOutOfRangeException>(
        [&] { (void)RandomAccess::Read(fd, two, -5); }, "fileOffset", "Read(vector, -5)");
    ExpectThrowsNaming<System::ArgumentOutOfRangeException>(
        [&] { RandomAccess::Write(fd, two, -5); }, "fileOffset", "Write(vector, -5)");
}

TEST_F(RandomAccessFixture, GetLengthThrowsForAnInvalidDescriptorInsteadOfReturningMinusOne) {
    // The sentinel -1 is a value no caller could distinguish from a real length, and one that
    // .NET can never produce.
    EXPECT_THROW((void)RandomAccess::GetLength(-1), IOException);
}

TEST_F(RandomAccessFixture, GetLengthAlsoThrowsForAVALIDButUnseekableDescriptor) {
    // WIDER THAN THE FINDING. SR-AUD-340 says "an invalid descriptor". Measured
    // (build-probe/2100_probe1_before.log), a perfectly valid PIPE returned -1 too, because
    // lseek() fails with ESPIPE and all three results were discarded. The repair covers both.
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    EXPECT_THROW((void)RandomAccess::GetLength(pipefd[0]), IOException);
    ::close(pipefd[0]);
    ::close(pipefd[1]);
}

TEST_F(RandomAccessFixture, EveryNativeFailureCarriesItsNativeReason) {
    // Before #2100 the reason was discarded and every failure read "RandomAccess::<x> failed",
    // so a caller could not tell EBADF from ENOSPC. The message must name the member AND carry
    // the operating system's own text.
    try {
        (void)RandomAccess::GetLength(-1);
        FAIL() << "expected a throw";
    } catch (const IOException& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("RandomAccess::GetLength"), std::string::npos) << message;
        EXPECT_NE(message.find("descriptor"), std::string::npos)
            << "the native reason (EBADF) must survive: " << message;
    }
    try {
        RandomAccess::FlushToDisk(-1);
        FAIL() << "expected a throw";
    } catch (const IOException& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("RandomAccess::FlushToDisk"), std::string::npos) << message;
        EXPECT_GT(message.size(), std::string("RandomAccess::FlushToDisk failed: ").size())
            << "a native reason must be appended: " << message;
    }
}

TEST_F(RandomAccessFixture, EveryPreviouslyACCEPTEDInputIsStillAccepted) {
    // The other half of the contract: #2100 must narrow the domain and nothing else.
    SharpRuntime::bytecs buf[8] = {};
    EXPECT_NO_THROW((void)RandomAccess::Read(fd, buf, 0, 0));
    EXPECT_NO_THROW(RandomAccess::Write(fd, buf, 0, 0));
    EXPECT_EQ(RandomAccess::Read(fd, buf, 4, 0), 4);
    EXPECT_EQ(RandomAccess::Read(fd, buf, 1, 0), 1);
    // Reading entirely past end of file is a zero-byte read, not an error.
    EXPECT_EQ(RandomAccess::Read(fd, buf, 4, 4096), 0);
    EXPECT_NO_THROW(RandomAccess::SetLength(fd, 0));
    EXPECT_NO_THROW(RandomAccess::FlushToDisk(fd));
}

TEST_F(RandomAccessFixture, AValidRoundTripIsByteIdenticalToBefore) {
    RandomAccess::SetLength(fd, 0);
    const SharpRuntime::bytecs payload[5] = {10, 20, 30, 40, 50};
    RandomAccess::Write(fd, payload, 5, 0);
    EXPECT_EQ(RandomAccess::GetLength(fd), 5);
    SharpRuntime::bytecs back[5] = {};
    EXPECT_EQ(RandomAccess::Read(fd, back, 5, 0), 5);
    for (int i = 0; i < 5; ++i) EXPECT_EQ(back[i], payload[i]) << "byte " << i;
    // Writing at a non-zero offset, and reading it back, are both unchanged.
    const SharpRuntime::bytecs tail[2] = {99, 98};
    RandomAccess::Write(fd, tail, 2, 3);
    SharpRuntime::bytecs after[5] = {};
    EXPECT_EQ(RandomAccess::Read(fd, after, 5, 0), 5);
    EXPECT_EQ(after[3], 99);
    EXPECT_EQ(after[4], 98);
}

TEST_F(RandomAccessFixture, GetLengthStillDoesNotDisturbTheFilePosition) {
    // The repair adds error checks to the three lseek() calls without changing the mechanism, so
    // the position must still be restored exactly. A pin, so a future switch to fstat() is a
    // decision rather than a drift.
    ASSERT_EQ(::lseek(fd, 2, SEEK_SET), 2);
    EXPECT_EQ(RandomAccess::GetLength(fd), 4);
    EXPECT_EQ(::lseek(fd, 0, SEEK_CUR), 2);
}

TEST_F(RandomAccessFixture, TwoHundredRejectedCallsLeakNoDescriptor) {
    // Direct accounting, per plan §14 — never LSan, which tracks memory and would say nothing
    // about a descriptor.
    SharpRuntime::bytecs buf[4] = {};
    const int before = fdCount();
    for (int i = 0; i < 50; ++i) {
        EXPECT_THROW(RandomAccess::Write(fd, buf, -1, 0), System::ArgumentOutOfRangeException);
        EXPECT_THROW((void)RandomAccess::Read(fd, buf, -1, 0), System::ArgumentOutOfRangeException);
        EXPECT_THROW((void)RandomAccess::GetLength(-1), IOException);
        EXPECT_THROW(RandomAccess::SetLength(fd, -1), System::ArgumentOutOfRangeException);
    }
    EXPECT_EQ(fdCount(), before) << "a rejected call must not open or lose a descriptor";
}

TEST_F(RandomAccessFixture, TheSameDescriptorIsFullyUsableAfterRepeatedRejections) {
    // "Repeated use after failure" from the IO validation matrix: a rejection must leave the
    // descriptor in exactly the state it was in.
    SharpRuntime::bytecs buf[4] = {};
    for (int i = 0; i < 10; ++i) {
        EXPECT_THROW(RandomAccess::Write(fd, buf, -1, 0), System::ArgumentOutOfRangeException);
        EXPECT_THROW((void)RandomAccess::Read(fd, buf, 2, -1), System::ArgumentOutOfRangeException);
    }
    SharpRuntime::bytecs back[4] = {};
    EXPECT_EQ(RandomAccess::Read(fd, back, 4, 0), 4);
    EXPECT_EQ(back[0], 1);
    EXPECT_EQ(back[3], 4);
    EXPECT_EQ(RandomAccess::GetLength(fd), 4);
}

TEST_F(RandomAccessFixture, AWriteToAReadOnlyDescriptorFailsWithItsNativeReason) {
    // SR-AUD-340's own report names "read-only descriptors" among the cases no test covered.
    // The rejection comes from the operating system rather than from an argument check, so what
    // #2100 changes here is that the reason survives into the message.
    const std::string readOnlyPath = under("readonly.bin");
    File::WriteAllText(readOnlyPath, "abcd");
    const int ro = ::open(readOnlyPath.c_str(), O_RDONLY);
    ASSERT_GE(ro, 0);
    const SharpRuntime::bytecs payload[2] = {1, 2};
    try {
        RandomAccess::Write(ro, payload, 2, 0);
        ADD_FAILURE() << "a write to a read-only descriptor must fail";
    } catch (const IOException& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("RandomAccess::Write"), std::string::npos) << message;
        EXPECT_GT(message.size(), std::string("RandomAccess::Write failed: ").size()) << message;
    }
    // Reading through the same descriptor still works, so the failure was the write, not the fd.
    SharpRuntime::bytecs back[4] = {};
    EXPECT_EQ(RandomAccess::Read(ro, back, 4, 0), 4);
    ::close(ro);
}

// ===========================================================================
// #2108 / SR-AUD-344 — UnmanagedMemoryStream records being closed and must enforce it
//
// Split out of #2098 by measurement: this type ALREADY has `bool isOpen_`, Close() already
// clears it, and getCanRead/getCanWrite/getCanSeek already consult it, so the repair is pure
// logic with ZERO object-layout change and needs no approval. The four text wrappers of #2098
// are a different matter and stay blocked on Approval IO-1.
// ===========================================================================

namespace {

/// A closed stream over a live buffer. The buffer outlives the stream in every test below, so a
/// failure here is about the stream's state, never about dangling memory.
class ClosedUnmanagedStream {
public:
    SharpRuntime::bytecs backing[4] = {1, 2, 3, 4};
    UnmanagedMemoryStream stream{backing, 4};
    ClosedUnmanagedStream() { stream.Close(); }
};

} // namespace

TEST(UnmanagedMemoryStreamClosedStateTests, EverySixMemberThatIgnoredTheClosedStateNowThrows) {
    // Measured before #2108 (build-probe/2098_probe2_traits.log): getLength returned 4,
    // setPosition(2) SUCCEEDED and getPosition then read back 2, while Read() already threw.
    ClosedUnmanagedStream c;
    EXPECT_THROW((void)c.stream.getLengthProperty(), System::ObjectDisposedException);
    EXPECT_THROW((void)c.stream.getCapacityProperty(), System::ObjectDisposedException);
    EXPECT_THROW((void)c.stream.getPositionProperty(), System::ObjectDisposedException);
    EXPECT_THROW(c.stream.setPositionProperty(2), System::ObjectDisposedException);
    EXPECT_THROW(c.stream.Flush(), System::ObjectDisposedException);
    // The sharpest one: this handed out a live pointer into the buffer of a CLOSED stream.
    EXPECT_THROW((void)c.stream.getPositionPointerProperty(), System::ObjectDisposedException);
}

TEST(UnmanagedMemoryStreamClosedStateTests, TheMembersThatWereAlreadyCorrectStayCorrect) {
    ClosedUnmanagedStream c;
    SharpRuntime::bytecs out[2] = {};
    EXPECT_THROW((void)c.stream.Read(out, 0, 2), System::ObjectDisposedException);
    EXPECT_THROW(c.stream.Write(out, 0, 2), System::ObjectDisposedException);
    EXPECT_THROW(c.stream.WriteByte(7), System::ObjectDisposedException);
    EXPECT_THROW(c.stream.SetLength(2), System::ObjectDisposedException);
    // The capability properties are the one closed-state surface that must NOT throw: they
    // answer "can I", and the answer for a closed stream is a plain false.
    EXPECT_FALSE(c.stream.getCanReadProperty());
    EXPECT_FALSE(c.stream.getCanWriteProperty());
    EXPECT_FALSE(c.stream.getCanSeekProperty());
}

TEST(UnmanagedMemoryStreamClosedStateTests, TheClosedCheckPRECEDESTheArgumentCheck) {
    // SetLength already threw for a closed stream -- but only AFTER rejecting a negative value,
    // so SetLength(-1) on a CLOSED stream reported the argument rather than the disposal. That
    // is the opposite order from Read/Write and from the .NET rule this module's own transcribed
    // note records. A bare EXPECT_THROW would not have caught it; the TYPE is what discriminates.
    ClosedUnmanagedStream c;
    EXPECT_THROW(c.stream.SetLength(-1), System::ObjectDisposedException);
    EXPECT_THROW(c.stream.setPositionProperty(-1), System::ObjectDisposedException);

    // On an OPEN stream the argument check is still reached and still reports the argument.
    SharpRuntime::bytecs backing[4] = {1, 2, 3, 4};
    UnmanagedMemoryStream open(backing, 4, 4, FileAccess::ReadWrite);
    EXPECT_THROW(open.SetLength(-1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(open.setPositionProperty(-1), System::ArgumentOutOfRangeException);
}

TEST(UnmanagedMemoryStreamClosedStateTests, DoubleCloseIsSafeAndAnOpenStreamIsUnaffected) {
    SharpRuntime::bytecs backing[4] = {10, 20, 30, 40};
    UnmanagedMemoryStream s(backing, 4, 4, FileAccess::ReadWrite);

    // Byte-identical behaviour while open.
    EXPECT_EQ(s.getLengthProperty(), 4);
    EXPECT_EQ(s.getCapacityProperty(), 4);
    EXPECT_EQ(s.getPositionProperty(), 0);
    EXPECT_EQ(s.getPositionPointerProperty(), backing);
    EXPECT_NO_THROW(s.Flush());
    s.setPositionProperty(2);
    EXPECT_EQ(s.getPositionProperty(), 2);
    EXPECT_EQ(s.getPositionPointerProperty(), backing + 2);
    s.setPositionProperty(0);
    SharpRuntime::bytecs out[4] = {};
    EXPECT_EQ(s.Read(out, 0, 4), 4);
    EXPECT_EQ(out[0], 10);
    EXPECT_EQ(out[3], 40);

    EXPECT_NO_THROW(s.Close());
    EXPECT_NO_THROW(s.Close()) << "double Close() must stay safe";
    EXPECT_THROW((void)s.getLengthProperty(), System::ObjectDisposedException);
}

TEST(UnmanagedMemoryStreamClosedStateTests, EveryRejectionCarriesTheSameClosedStreamMessage) {
    // One door, one message: a caller must not be able to tell which member rejected it.
    ClosedUnmanagedStream c;
    const std::string expected = "Cannot access a closed Stream.";
    auto messageOf = [](auto&& call) {
        try { call(); } catch (const System::ObjectDisposedException& e) { return std::string(e.what()); }
        return std::string("<did not throw ObjectDisposedException>");
    };
    EXPECT_NE(messageOf([&] { (void)c.stream.getLengthProperty(); }).find(expected), std::string::npos);
    EXPECT_NE(messageOf([&] { (void)c.stream.getCapacityProperty(); }).find(expected), std::string::npos);
    EXPECT_NE(messageOf([&] { (void)c.stream.getPositionProperty(); }).find(expected), std::string::npos);
    EXPECT_NE(messageOf([&] { (void)c.stream.getPositionPointerProperty(); }).find(expected), std::string::npos);
    EXPECT_NE(messageOf([&] { c.stream.Flush(); }).find(expected), std::string::npos);
}

// ===========================================================================
// #2098's layout PIN — landed early, by #2108
//
// #2098's acceptance criteria require "a layout pin covering all five types plus the two base
// classes". Landing it NOW, while #2098 is still blocked on Approval IO-1, is what makes that
// approval's cost auditable: the approval sentence quotes StringWriter 384 -> 392 and says the
// other three are unchanged, and this pin is what proves the "before" half of that claim.
//
// It deliberately pins RELATIONSHIPS and a probe-struct comparison rather than literal byte
// counts wherever it can, per docs/SystemNetWebSocketsNamespaceReviewPlan.md §11.
// ===========================================================================

namespace {

/// Shadow shapes matching the real classes field-for-field. If a real class gains, loses or
/// reorders a member, its shadow stops matching and the pin fails -- which is the point.
struct ShadowTextReader { virtual ~ShadowTextReader() = default; virtual SharpRuntime::intcs Read() { return -1; } };
struct ShadowTextWriter { virtual ~ShadowTextWriter() = default; virtual void Write(const std::string&) {} };
struct ShadowStringReader : ShadowTextReader { std::string s_; SharpRuntime::intcs pos_ = 0; bool closed_ = false; };
struct ShadowStreamReader : ShadowTextReader {
    Stream* stream_; bool leaveOpen_; bool ownsStream_; bool hasPeeked_; SharpRuntime::bytecs peeked_;
    bool closed_ = false;
};
struct ShadowStreamWriter : ShadowTextWriter {
    Stream* stream_; bool leaveOpen_; bool ownsStream_; bool closed_ = false;
};
struct ShadowUnmanaged {
    virtual ~ShadowUnmanaged() = default;
    SharpRuntime::bytecs* buffer_; SharpRuntime::intcs length_; SharpRuntime::intcs capacity_;
    SharpRuntime::intcs position_; FileAccess access_; bool isOpen_;
};

} // namespace

TEST(IoLayoutPinTests, TheBaseClassesCarryNoDataAndAreVtablePointerOnly) {
    // THE load-bearing fact behind Approval IO-1's recommendation: TextReader and TextWriter have
    // NO data members, so option (b) -- a bool in the base -- would double them, 8 -> 16, and
    // change every derived type at once. If this ever stops holding, that trade-off changed.
    EXPECT_EQ(sizeof(TextReader), sizeof(void*));
    EXPECT_EQ(sizeof(TextWriter), sizeof(void*));
    EXPECT_EQ(sizeof(Stream), sizeof(void*));
    EXPECT_EQ(sizeof(TextReader), sizeof(ShadowTextReader));
    EXPECT_EQ(sizeof(TextWriter), sizeof(ShadowTextWriter));
}

TEST(IoLayoutPinTests, TheFiveWrapperLayoutsAreExactlyWhatApprovalIO1WasCostedAgainst) {
    EXPECT_EQ(sizeof(StringReader), sizeof(ShadowStringReader));
    EXPECT_EQ(sizeof(StreamReader), sizeof(ShadowStreamReader));
    EXPECT_EQ(sizeof(StreamWriter), sizeof(ShadowStreamWriter));
    EXPECT_EQ(sizeof(UnmanagedMemoryStream), sizeof(ShadowUnmanaged));
    // StringWriter is the ONE type Approval IO-1 said would grow, and #2098 has now made it
    // grow: 384 -> 392, the flag plus its alignment padding. Its size is dominated by
    // std::ostringstream, a standard-library implementation detail, so it is pinned as a
    // relationship rather than a literal: the base's vtable pointer, plus the stream, plus one
    // aligned slot for `closed_`.
    EXPECT_EQ(sizeof(StringWriter),
              sizeof(void*) + sizeof(std::ostringstream) + alignof(std::ostringstream))
        << "Approval IO-1 costed StringWriter at exactly one extra aligned slot";
    EXPECT_EQ(alignof(StringWriter), alignof(std::ostringstream));
}

TEST(IoLayoutPinTests, ThreeOfTheFourTextWrappersPAIDNOTHINGForApprovalIO1sFlag) {
    // Approval IO-1 claimed a private bool costs StringReader, StreamReader and StreamWriter
    // NOTHING because it lands in existing tail padding. #2098 has landed the flag, so this is
    // no longer a prediction: it compares each real type against a shadow that has the flag and
    // against the same shadow WITHOUT it. Both must match, which is only possible if the flag
    // really was free.
    struct NoFlagStringReader : ShadowTextReader { std::string s_; SharpRuntime::intcs pos_ = 0; };
    struct NoFlagStreamReader : ShadowTextReader {
        Stream* stream_; bool leaveOpen_; bool ownsStream_; bool hasPeeked_;
        SharpRuntime::bytecs peeked_;
    };
    struct NoFlagStreamWriter : ShadowTextWriter { Stream* stream_; bool leaveOpen_; bool ownsStream_; };
    EXPECT_EQ(sizeof(StringReader), sizeof(NoFlagStringReader))
        << "StringReader's flag was costed as free";
    EXPECT_EQ(sizeof(StreamReader), sizeof(NoFlagStreamReader))
        << "StreamReader's flag was costed as free";
    EXPECT_EQ(sizeof(StreamWriter), sizeof(NoFlagStreamWriter))
        << "StreamWriter's flag was costed as free";
}

TEST(IoLayoutPinTests, BothBaseClassesAlreadyExposeAVirtualCloseSoNoVtableSlotIsNeeded) {
    // plan §7.1 and #2098's notes both said "TextWriter has no Close() at all". It does. This
    // pin is why that premise cannot silently come back: taking the address of the member proves
    // it exists, and calling it through a base reference proves it is virtual and non-pure.
    void (TextReader::*readerClose)() = &TextReader::Close;
    void (TextWriter::*writerClose)() = &TextWriter::Close;
    EXPECT_NE(readerClose, nullptr);
    EXPECT_NE(writerClose, nullptr);
    StringReader reader("x");
    TextReader& asBase = reader;
    EXPECT_NO_THROW(asBase.Close()) << "TextReader::Close() is a non-pure virtual no-op";
}


// ===========================================================================
// #2098 / SR-AUD-337 + SR-AUD-343 — the four text wrappers enforce their closed state
//
// Landed under Approval IO-1, granted as `docs/StandingApprovals.md` SA-3 (2026-08-17).
// The layout cost is exactly what the approval quoted: StringWriter 384 -> 392, the other
// three free — asserted by the pins above, not by this section.
//
// The reference tree IS available for this ticket (docs/StandingApprovals.md §5.1), so unlike
// the sections above, every exception identity and message below is a VERIFIED match to
// .NET rather than this port's own choice. It is read from a .NET 11 preview snapshot.
//
// The measurement that changed the design: .NET's two stream wrappers DISAGREE about whether
// `leaveOpen` also suppresses disposal of the wrapper.
//
//   StreamReader.cs:243-268    _disposed = true;  THEN  if (_closable) { _stream.Close(); }
//   StreamWriter.cs:221-244    if (_closable && !_disposed) { ... finally { _disposed = true; } }
//
// So a leaveOpen READER is disposed by Close() and a leaveOpen WRITER is not. SR-AUD-337
// reported both halves as divergences; only the reader half is one.
// ===========================================================================

namespace {

/// Counts Close() calls, so "the destructor must not close a stream the explicit Close()
/// already closed" is a measurable claim rather than an assertion about intent.
class CloseCountingStream final : public System::IO::Stream {
public:
    int closeCount = 0;
    int flushCount = 0;
    std::string written;
    std::string source;
    std::size_t readPos = 0;

    intcs Read(SharpRuntime::bytecs* buffer, intcs offset, intcs count) override {
        intcs n = 0;
        while (n < count && readPos < source.size()) {
            buffer[offset + n] = static_cast<SharpRuntime::bytecs>(source[readPos++]);
            ++n;
        }
        return n;
    }
    void Write(const SharpRuntime::bytecs* buffer, intcs offset, intcs count) override {
        for (intcs i = 0; i < count; ++i) written.push_back(static_cast<char>(buffer[offset + i]));
    }
    void Close() override { ++closeCount; }
    void Flush() override { ++flushCount; }
    [[nodiscard]] intcs getLengthProperty() const override {
        return static_cast<intcs>(source.size());
    }
    [[nodiscard]] bool getCanWriteProperty() const override { return true; }
};

/// The exact text .NET produces for each of the four, per SR.ObjectDisposed_ReaderClosed /
/// SR.ObjectDisposed_WriterClosed and each type's choice of object name.
constexpr const char* kStringReaderClosed = "Cannot read from a closed TextReader.";
constexpr const char* kStringWriterClosed = "Cannot write to a closed TextWriter.";
constexpr const char* kStreamReaderClosed =
    "Cannot read from a closed TextReader.\nObject name: 'StreamReader'.";
constexpr const char* kStreamWriterClosed =
    "Cannot write to a closed TextWriter.\nObject name: 'StreamWriter'.";

} // namespace

TEST(TextWrapperClosedStateTests, StringReaderRejectsEveryReadMemberAfterClose) {
    // Before #2098 this reader was ENTIRELY unaffected by Close(): the audit measured
    // Peek()=104, Read()=104 and ReadToEnd()="ello" after closing a StringReader("hello")
    // that had already consumed one character.
    StringReader r("hello");
    EXPECT_EQ(r.Read(), 'h');
    r.Close();
    EXPECT_THROW((void)r.Peek(), System::ObjectDisposedException);
    EXPECT_THROW((void)r.Read(), System::ObjectDisposedException);
    EXPECT_THROW((void)r.ReadLine(), System::ObjectDisposedException);
    EXPECT_THROW((void)r.ReadToEnd(), System::ObjectDisposedException);
}

TEST(TextWrapperClosedStateTests, StringReaderCarriesDotNetsExactMessageAndItsNullObjectName) {
    // StringReader.cs:325-328 passes objectName = null, so there is NO "Object name:" suffix.
    // This is the half that distinguishes it from StreamReader, and it is easy to get wrong.
    StringReader r("hello");
    r.Close();
    try {
        (void)r.Read();
        FAIL() << "expected ObjectDisposedException";
    } catch (const System::ObjectDisposedException& e) {
        EXPECT_STREQ(e.what(), kStringReaderClosed);
        EXPECT_EQ(e.getObjectNameProperty(), std::string());
    }
}

TEST(TextWrapperClosedStateTests, StringReaderClosingTwiceIsSafeAndAnOpenReaderIsUnaffected) {
    StringReader closed("hello");
    closed.Close();
    EXPECT_NO_THROW(closed.Close());
    EXPECT_THROW((void)closed.Read(), System::ObjectDisposedException);

    StringReader open("hello");
    EXPECT_EQ(open.Peek(), 'h');
    EXPECT_EQ(open.ReadLine(), "hello");
    EXPECT_EQ(open.ReadToEnd(), "");
}

TEST(TextWrapperClosedStateTests, StringWriterRejectsEveryWriteOverloadAfterClose) {
    // Every inherited overload funnels through Write(const std::string&), which is how one
    // guard covers what .NET spells as nine separate _isOpen checks.
    StringWriter w;
    w.Write(std::string("kept"));
    w.Close();
    EXPECT_THROW(w.Write(std::string("x")), System::ObjectDisposedException);
    EXPECT_THROW(w.Write("literal"), System::ObjectDisposedException);
    EXPECT_THROW(w.Write('c'), System::ObjectDisposedException);
    EXPECT_THROW(w.Write(static_cast<SharpRuntime::intcs>(7)), System::ObjectDisposedException);
    EXPECT_THROW(w.Write(true), System::ObjectDisposedException);
    EXPECT_THROW(w.WriteLine(), System::ObjectDisposedException);
    EXPECT_THROW(w.WriteLine(std::string("x")), System::ObjectDisposedException);
}

TEST(TextWrapperClosedStateTests, StringWriterKEEPSItsTextReadableAfterClose) {
    // DELIBERATELY not guarded. StringWriter.cs:309-312 defines ToString() as a bare
    // `return _sb.ToString();` and :64-67 GetStringBuilder() as a bare `return _sb;`, neither
    // of which consults _isOpen. Guarding them would be a divergence, not a stricter repair.
    StringWriter w;
    w.Write(std::string("kept"));
    w.Close();
    EXPECT_EQ(w.ToString(), "kept");
    EXPECT_EQ(w.GetStringBuilder(), "kept");
    EXPECT_NO_THROW(w.Close());
    EXPECT_EQ(w.ToString(), "kept") << "a second Close() must not discard the buffer either";
}

TEST(TextWrapperClosedStateTests, StringWriterCarriesDotNetsExactMessageAndItsNullObjectName) {
    StringWriter w;
    w.Close();
    try {
        w.Write(std::string("x"));
        FAIL() << "expected ObjectDisposedException";
    } catch (const System::ObjectDisposedException& e) {
        EXPECT_STREQ(e.what(), kStringWriterClosed);
        EXPECT_EQ(e.getObjectNameProperty(), std::string());
    }
}

TEST(TextWrapperClosedStateTests, StreamReaderIsDisposedByCloseEVENWithLeaveOpen) {
    // The headline of SR-AUD-337's reader half: the audit measured Read() returning 97 after
    // Close() on a leaveOpen reader. .NET sets _disposed OUTSIDE its _closable test, so the
    // reader is disposed while the stream it does not own stays open.
    CloseCountingStream s;
    s.source = "abc";
    StreamReader r(&s, /*leaveOpen=*/true);
    EXPECT_EQ(r.Read(), 'a');
    r.Close();

    EXPECT_EQ(s.closeCount, 0) << "leaveOpen still governs the STREAM";
    EXPECT_THROW((void)r.Peek(), System::ObjectDisposedException);
    EXPECT_THROW((void)r.Read(), System::ObjectDisposedException);
    EXPECT_THROW((void)r.ReadLine(), System::ObjectDisposedException);
    EXPECT_THROW((void)r.ReadToEnd(), System::ObjectDisposedException);

    // ...and the stream really is still usable by whoever kept ownership of it.
    SharpRuntime::bytecs buf[1] = {0};
    EXPECT_EQ(s.Read(buf, 0, 1), 1);
    EXPECT_EQ(buf[0], static_cast<SharpRuntime::bytecs>('b'));
}

TEST(TextWrapperClosedStateTests, StreamReaderCarriesItsTYPENAMEWhereStringReaderCarriesNone) {
    CloseCountingStream s;
    s.source = "abc";
    StreamReader r(&s, true);
    r.Close();
    try {
        (void)r.Read();
        FAIL() << "expected ObjectDisposedException";
    } catch (const System::ObjectDisposedException& e) {
        EXPECT_STREQ(e.what(), kStreamReaderClosed);
        EXPECT_EQ(e.getObjectNameProperty(), "StreamReader")
            << "StreamReader.cs:1408 passes GetType().Name; StringReader.cs:327 passes null";
    }
}

TEST(TextWrapperClosedStateTests, StreamReaderBaseStreamStaysReadableAfterCloseAndCloseIsIdempotent) {
    CloseCountingStream s;
    s.source = "abc";
    {
        StreamReader r(&s, /*leaveOpen=*/false);
        EXPECT_EQ(r.getBaseStreamProperty(), &s);
        r.Close();
        EXPECT_EQ(s.closeCount, 1);
        // .NET's BaseStream (StreamReader.cs:274) is a bare `=> _stream` with no disposal
        // check, so this member deliberately keeps working.
        EXPECT_EQ(r.getBaseStreamProperty(), &s);
        r.Close();
        EXPECT_EQ(s.closeCount, 1) << "Dispose returns early when already disposed";
    }
    EXPECT_EQ(s.closeCount, 1) << "the destructor must not re-close what Close() already closed";
}

TEST(TextWrapperClosedStateTests, StreamWriterKEEPSWORKINGAfterCloseWithLeaveOpen_MatchingDotNet) {
    // SR-AUD-337's writer half is a FALSE POSITIVE against the reference, and this pins that.
    // StreamWriter.cs assigns _disposed only inside `if (_closable && !_disposed)`, and
    // _closable is !leaveOpen — so a leaveOpen writer is never disposed and its post-Close
    // writes are correct behaviour. The audit measured "the stream grows by 11 bytes"; .NET
    // grows it too.
    CloseCountingStream s;
    StreamWriter w(&s, /*leaveOpen=*/true);
    w.Write(std::string("before"));
    w.Close();
    EXPECT_EQ(s.closeCount, 0);
    EXPECT_NO_THROW(w.Write(std::string("-after")));
    EXPECT_NO_THROW(w.Flush());
    EXPECT_EQ(s.written, "before-after");
}

TEST(TextWrapperClosedStateTests, StreamWriterRejectsWritesAfterCloseWhenItOwnsTheClose) {
    CloseCountingStream s;
    StreamWriter w(&s, /*leaveOpen=*/false);
    w.Write(std::string("before"));
    w.Close();
    EXPECT_EQ(s.closeCount, 1);
    EXPECT_THROW(w.Write(std::string("x")), System::ObjectDisposedException);
    EXPECT_THROW(w.Write("literal"), System::ObjectDisposedException);
    EXPECT_THROW(w.Write('c'), System::ObjectDisposedException);
    EXPECT_THROW(w.WriteLine(), System::ObjectDisposedException);
    EXPECT_THROW(w.Flush(), System::ObjectDisposedException);
    EXPECT_EQ(s.written, "before") << "nothing reached the stream after Close()";
}

TEST(TextWrapperClosedStateTests, StreamWriterCarriesItsTypeNameAndStillAcceptsANullPointer) {
    CloseCountingStream s;
    StreamWriter w(&s, false);
    w.Close();
    try {
        w.Write(std::string("x"));
        FAIL() << "expected ObjectDisposedException";
    } catch (const System::ObjectDisposedException& e) {
        EXPECT_STREQ(e.what(), kStreamWriterClosed);
        EXPECT_EQ(e.getObjectNameProperty(), "StreamWriter");
    }
    // .NET's Write(string?) returns before its disposal check when the value is null
    // (TextWriter.cs:277-283), so a null pointer stays a silent no-op even on a closed writer.
    const char* nothing = nullptr;
    EXPECT_NO_THROW(w.Write(nothing));
    EXPECT_EQ(w.getBaseStreamProperty(), &s) << "BaseStream has no disposal check either";
}

TEST(TextWrapperClosedStateTests, StreamWriterDestructorDoesNotRECloseAfterAnExplicitClose) {
    CloseCountingStream s;
    {
        StreamWriter w(&s, /*leaveOpen=*/false);
        w.Close();
        EXPECT_EQ(s.closeCount, 1);
        w.Close();
        EXPECT_EQ(s.closeCount, 1);
    }
    EXPECT_EQ(s.closeCount, 1);
}


// ===========================================================================
// #2099 / SR-AUD-342 (surviving half) — FileStream members that ignored Close()
//
// SR-AUD-342 named "OpenOrCreate+Read can write a new file, and closed metadata remains usable".
// Plan §6.2 measured the write half as NO LONGER REPRODUCIBLE and scoped this ticket to the
// closed-state half. Measured again for #2099 (build-probe/2099_probe1_before.log), that half is
// SIX members, not the three §6.2 lists, and one of the six fails in a way §6.2 did not predict.
//
// LAYOUT-NEUTRAL, and that is why this did not wait for #2098's Approval IO-1: FileStream needs
// no `closed_` flag because file_.is_open() already IS one. sizeof stayed 576 across the repair.
// ===========================================================================

namespace {

/// A closed FileStream over a real 5-byte file that still exists on disk. The file outliving the
/// stream is the point: every rejection below is about the stream's state, never about a missing
/// file, and getLengthProperty() could still have answered from the path if it were allowed to.
class ClosedFileStreamFixture : public IoReviewFixture {
protected:
    std::filesystem::path path;

    void SetUp() override {
        IoReviewFixture::SetUp();
        path = root / "subject.bin";
        FileStream seed(path.string(), FileMode::Create, FileAccess::Write);
        const SharpRuntime::bytecs data[] = {'h', 'e', 'l', 'l', 'o'};
        seed.Write(data, 0, 5);
        seed.Close();
    }

    FileStream opened() { return FileStream(path.string(), FileMode::Open, FileAccess::Read); }
};

} // namespace

TEST_F(ClosedFileStreamFixture, TheSixMembersThatIgnoredTheClosedStateNowThrow) {
    // Measured before the repair (build-probe/2099_probe1_before.log): getLength returned 5 by
    // re-stat'ing path_, getPosition returned -1, and setPosition/Seek/Flush all SUCCEEDED.
    FileStream fs = opened();
    fs.Close();
    EXPECT_THROW((void)fs.getLengthProperty(), System::ObjectDisposedException);
    EXPECT_THROW((void)fs.getPositionProperty(), System::ObjectDisposedException);
    EXPECT_THROW(fs.setPositionProperty(0), System::ObjectDisposedException);
    EXPECT_THROW((void)fs.Seek(0, SeekOrigin::Begin), System::ObjectDisposedException);
    EXPECT_THROW((void)fs.Seek(0, SeekOrigin::End), System::ObjectDisposedException);
    EXPECT_THROW(fs.Flush(), System::ObjectDisposedException);
}

TEST_F(ClosedFileStreamFixture, SeekCurrentReportedTheWrongDIAGNOSTICNotMerelyTheWrongOutcome) {
    // The one §6.2 did not predict. §6.2 says "Seek() succeeds outright"; for SeekOrigin::Current
    // it did not succeed — it threw IOException("...before the beginning of the stream."), because
    // getPositionProperty()'s -1 sentinel does not stay local: Stream::Seek adds it to the offset,
    // making the sum negative. So a closed FileStream reported a complaint about the SEEK TARGET.
    // A bare EXPECT_THROW would have passed before the repair; only the TYPE discriminates.
    FileStream fs = opened();
    fs.Close();
    EXPECT_THROW((void)fs.Seek(0, SeekOrigin::Current), System::ObjectDisposedException);
}

TEST_F(ClosedFileStreamFixture, TheClosedCheckPRECEDESTheArgumentCheck) {
    // Two members tested their argument first, so a closed stream reported the argument rather
    // than the disposal — the same reversal #2108 corrected in UnmanagedMemoryStream.
    FileStream fs = opened();
    fs.Close();
    EXPECT_THROW(fs.setPositionProperty(-1), System::ObjectDisposedException);
    EXPECT_THROW(fs.SetLength(-1), System::ObjectDisposedException);

    // On an OPEN stream the argument check is still reached and still reports the argument.
    FileStream open(path.string(), FileMode::Open, FileAccess::ReadWrite);
    EXPECT_THROW(open.setPositionProperty(-1), System::ArgumentOutOfRangeException);
    EXPECT_THROW(open.SetLength(-1), System::ArgumentOutOfRangeException);
}

TEST_F(ClosedFileStreamFixture, TheMembersThatWereAlreadyCorrectStayCorrect) {
    FileStream fs = opened();
    fs.Close();
    SharpRuntime::bytecs out[2] = {};
    EXPECT_THROW((void)fs.Read(out, 0, 2), System::ObjectDisposedException);
    EXPECT_THROW(fs.SetLength(0), System::ObjectDisposedException);
    // The capability properties answer "can I", so a closed stream returns a plain false.
    EXPECT_FALSE(fs.getCanReadProperty());
    EXPECT_FALSE(fs.getCanWriteProperty());
    EXPECT_FALSE(fs.getCanSeekProperty());
    EXPECT_FALSE(fs.IsOpen());
    EXPECT_NO_THROW(fs.Close()); // double Close() stays safe
}

TEST_F(ClosedFileStreamFixture, EveryRejectionCarriesTheSameClosedFileMessage) {
    // One EnsureNotClosed() funnel means one message. If a member ever grows its own throw again,
    // this is what fails.
    FileStream fs = opened();
    fs.Close();
    const std::string expected = "Cannot access a closed file.";
    auto messageOf = [&](auto&& call) {
        try { call(); } catch (const System::ObjectDisposedException& e) { return std::string(e.what()); }
        return std::string("<no throw>");
    };
    EXPECT_NE(messageOf([&]{ (void)fs.getLengthProperty(); }).find(expected), std::string::npos);
    EXPECT_NE(messageOf([&]{ (void)fs.getPositionProperty(); }).find(expected), std::string::npos);
    EXPECT_NE(messageOf([&]{ fs.setPositionProperty(0); }).find(expected), std::string::npos);
    EXPECT_NE(messageOf([&]{ fs.Flush(); }).find(expected), std::string::npos);
    EXPECT_NE(messageOf([&]{ fs.SetLength(0); }).find(expected), std::string::npos);
}

TEST_F(ClosedFileStreamFixture, AnOpenStreamIsCompletelyUNAFFECTED) {
    // The whole open-stream control block of build-probe/2099_probe1_*.log is byte-identical
    // before and after the repair. This is that block as a permanent regression.
    FileStream fs = opened();
    EXPECT_EQ(fs.getLengthProperty(), 5);
    EXPECT_EQ(fs.getPositionProperty(), 0);
    EXPECT_EQ(fs.Seek(2, SeekOrigin::Begin), 2);
    EXPECT_EQ(fs.getPositionProperty(), 2);
    EXPECT_EQ(fs.Seek(0, SeekOrigin::End), 5);
    EXPECT_EQ(fs.Seek(-1, SeekOrigin::Current), 4);
    EXPECT_NO_THROW(fs.Flush());
    EXPECT_TRUE(fs.IsOpen());

    // A read still returns the original bytes: nothing in this repair touched the data path.
    fs.setPositionProperty(0);
    SharpRuntime::bytecs back[5] = {};
    EXPECT_EQ(fs.Read(back, 0, 5), 5);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(back), 5), "hello");
}

TEST_F(ClosedFileStreamFixture, TheFileOnDiskIsNeverTouchedByARejectedCall) {
    // setPositionProperty and SetLength both reach path_-based work once past their guards, so a
    // rejection that ran anyway would be observable on disk. It is not: the file stays 5 bytes.
    FileStream fs = opened();
    fs.Close();
    for (int i = 0; i < 50; ++i) {
        EXPECT_THROW(fs.SetLength(0), System::ObjectDisposedException);
        EXPECT_THROW(fs.setPositionProperty(0), System::ObjectDisposedException);
        EXPECT_THROW(fs.Flush(), System::ObjectDisposedException);
    }
    EXPECT_EQ(std::filesystem::file_size(path), 5u);
}

TEST_F(ClosedFileStreamFixture, TheRESIDUALSeekOrderingCaseIsSHAREDWithItsSiblingNotSpecificToFileStream) {
    // FileStream deliberately does NOT override Seek. Stream::Seek tests a negative RESULTING
    // position before delegating, so on a CLOSED stream Seek(-1, Begin) still reports IOException
    // rather than ObjectDisposedException. Measured (build-probe/2099_probe2_seek_residue.log),
    // UnmanagedMemoryStream — repaired by #2108 — does exactly the same. Overriding Seek in
    // FileStream alone would CREATE a divergence between two siblings that agree today, so the
    // residue is pinned here rather than removed. Changing Stream::Seek is not this ticket.
    FileStream fs = opened();
    fs.Close();
    EXPECT_THROW((void)fs.Seek(-1, SeekOrigin::Begin), System::IO::IOException);

    SharpRuntime::bytecs backing[4] = {1, 2, 3, 4};
    UnmanagedMemoryStream ums(backing, 4);
    ums.Close();
    EXPECT_THROW((void)ums.Seek(-1, SeekOrigin::Begin), System::IO::IOException);
}

TEST_F(ClosedFileStreamFixture, RepeatedRejectionsLeakNoDescriptor) {
    // Plan §6.2 recorded a measured positive — FileStream leaks no descriptor over 20 double-close
    // cycles — and #2099's acceptance criteria asks for the number to be REPORTED, not asserted.
    auto fdCount = [] {
        int n = 0; std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator("/proc/self/fd", ec)) { (void)e; ++n; }
        return n;
    };
    const int before = fdCount();
    for (int i = 0; i < 100; ++i) {
        FileStream fs(path.string(), FileMode::Open, FileAccess::Read);
        fs.Close();
        EXPECT_THROW((void)fs.getLengthProperty(), System::ObjectDisposedException);
        EXPECT_THROW((void)fs.getPositionProperty(), System::ObjectDisposedException);
        EXPECT_THROW(fs.Flush(), System::ObjectDisposedException);
        fs.Close();
    }
    const int after = fdCount();
    RecordProperty("fd_before", before);
    RecordProperty("fd_after", after);
    EXPECT_EQ(after, before) << "descriptor delta across 100 closed-stream rejection cycles";
}

// ===========================================================================================
// #2344 / SR-AUD-339, cause I-E, split from #2102 — FileSystemWatcher::setPathProperty never
// re-armed the inotify watch, and raced the watcher thread while doing it.
//
// Before-state, measured (build-probe/2102_probe1_before.log): after setPathProperty(B) on an
// enabled watcher, a file created in the OLD directory A was still reported, and its FullPath
// was built from the NEW directory — "B/sentinelOnA.txt", naming a file that does not exist.
// A file created in B raised nothing at all.
//
// The finding called that a stale watch. It is also UNDEFINED BEHAVIOUR: watchLoop() reads
// directory_ on the watcher thread while setPathProperty() writes that std::string on the
// caller thread. ThreadSanitizer reported 3 data races against the pre-repair tree and 0 after
// (build-probe/2102_probe3_tsan_before.log / _after.log, 400 path flips, 1,749 events).
//
// DETERMINISM: no sleep is used as synchronisation anywhere below. Every wait is on an EVENT,
// with a deadline that exists only so a wedged watcher FAILS instead of hanging. Handlers run
// on the watcher thread, so each one captures any exception it throws and the test surfaces it
// rather than letting it reach std::terminate.
// ===========================================================================================

namespace {

#if defined(__linux__)

/// Collects watcher callbacks off the watcher thread and lets a test wait for a NAMED event.
class WatchRecorder {
public:
    struct Seen {
        WatcherChangeTypes type;
        std::string name;
        std::string fullPath;
    };

    void add(WatcherChangeTypes t, const std::string& n, const std::string& fp) {
        try {
            {
                std::lock_guard<std::mutex> lock(m_);
                events_.push_back(Seen{t, n, fp});
            }
            cv_.notify_all();
        } catch (...) {
            // A handler runs on the watcher thread: letting anything escape it would call
            // std::terminate and take the whole test executable down with no diagnostic.
            std::lock_guard<std::mutex> lock(m_);
            handlerFailed_ = true;
        }
    }

    /// True when an event named @p name has been observed. The deadline is a failsafe, never a
    /// synchronisation mechanism: a test that relies on it timing out says so at the call site.
    bool awaitName(const std::string& name,
                   std::chrono::milliseconds budget = std::chrono::milliseconds(5000)) {
        std::unique_lock<std::mutex> lock(m_);
        return cv_.wait_for(lock, budget, [&] { return findLocked(name) != nullptr; });
    }

    [[nodiscard]] bool sawName(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_);
        return findLocked(name) != nullptr;
    }

    /// Name alone is not enough once one file carries several event kinds (a file that is
    /// created and then modified produces two events with the same Name).
    bool awaitEvent(WatcherChangeTypes t, const std::string& name,
                    std::chrono::milliseconds budget = std::chrono::milliseconds(5000)) {
        std::unique_lock<std::mutex> lock(m_);
        return cv_.wait_for(lock, budget, [&] { return findLocked(t, name) != nullptr; });
    }

    [[nodiscard]] bool sawEvent(WatcherChangeTypes t, const std::string& name) {
        std::lock_guard<std::mutex> lock(m_);
        return findLocked(t, name) != nullptr;
    }

    [[nodiscard]] std::size_t size() {
        std::lock_guard<std::mutex> lock(m_);
        return events_.size();
    }

    [[nodiscard]] std::string fullPathOf(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_);
        const Seen* s = findLocked(name);
        return s ? s->fullPath : std::string();
    }

    [[nodiscard]] bool handlerFailed() {
        std::lock_guard<std::mutex> lock(m_);
        return handlerFailed_;
    }

private:
    const Seen* findLocked(const std::string& name) const {
        for (const auto& e : events_) {
            if (e.name == name) return &e;
        }
        return nullptr;
    }

    const Seen* findLocked(WatcherChangeTypes t, const std::string& name) const {
        for (const auto& e : events_) {
            if (e.type == t && e.name == name) return &e;
        }
        return nullptr;
    }

    std::mutex m_;
    std::condition_variable cv_;
    std::vector<Seen> events_;
    bool handlerFailed_ = false;
};

void subscribeAll(FileSystemWatcher& w, WatchRecorder& r) {
    w.Created.push_back([&r](void*, const FileSystemEventArgs& e) {
        r.add(e.getChangeTypeProperty(), e.getNameProperty(), e.getFullPathProperty());
    });
    w.Deleted.push_back([&r](void*, const FileSystemEventArgs& e) {
        r.add(e.getChangeTypeProperty(), e.getNameProperty(), e.getFullPathProperty());
    });
    w.Changed.push_back([&r](void*, const FileSystemEventArgs& e) {
        r.add(e.getChangeTypeProperty(), e.getNameProperty(), e.getFullPathProperty());
    });
    w.Renamed.push_back([&r](void*, const RenamedEventArgs& e) {
        r.add(e.getChangeTypeProperty(), e.getNameProperty(), e.getFullPathProperty());
    });
}

/// Creates an empty file: exactly one in-mask inotify event (IN_CREATE).
void touchNew(const std::filesystem::path& p) {
    const int fd = ::open(p.c_str(), O_CREAT | O_WRONLY, 0644);
    ASSERT_GE(fd, 0) << "could not create " << p;
    ::close(fd);
}

/// Appends one byte to an existing file: exactly one in-mask inotify event (IN_MODIFY).
void appendByte(const std::filesystem::path& p) {
    const int fd = ::open(p.c_str(), O_WRONLY | O_APPEND);
    ASSERT_GE(fd, 0) << "could not open " << p;
    const char c = 'x';
    const ssize_t n = ::write(fd, &c, 1);
    ASSERT_EQ(n, 1);
    ::close(fd);
}

/// Waits until an atomic counter becomes non-zero, or the deadline expires.
///
/// Ticket #2352. The three call sites below used `for (int i = 0; i < 2000; ++i) yield()`, which
/// is a bounded SPIN, not a wait: 2,000 yields is however long 2,000 yields happen to take, so on
/// a loaded machine it can expire while the watcher thread has not been scheduled even once. That
/// is exactly how AnExceptionEscapingAHandlerReachesErrorInsteadOfTerminating failed inside a
/// full-suite run while passing every time in isolation. The deadline here is a FAILSAFE, never a
/// synchronisation mechanism -- the same convention WatchRecorder::awaitName already documents.
bool awaitCounter(const std::atomic<int>& counter,
                  std::chrono::milliseconds budget = std::chrono::milliseconds(5000)) {
    const auto deadline = std::chrono::steady_clock::now() + budget;
    while (counter.load() == 0) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

/// Reads one byte from an existing file: exactly one in-mask inotify event (IN_ACCESS), and the
/// only way to observe #2346's decision 4(c) from a test.
void readByte(const std::filesystem::path& p) {
    const int fd = ::open(p.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0) << "could not open " << p;
    char c = 0;
    const ssize_t n = ::read(fd, &c, 1);
    ASSERT_GE(n, 0);
    ::close(fd);
}

/// The number of descriptors this process currently holds. Declared here rather than reused
/// from the #2100 fixture, which owns its copy as a private static member.
int watcherFdCount() {
    int n = 0;
    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator("/proc/self/fd", ec)) {
        (void)entry;
        ++n;
    }
    return n;
}

class WatcherReconfigurationFixture : public IoReviewFixture {
protected:
    std::filesystem::path dirA, dirB;

    void SetUp() override {
        IoReviewFixture::SetUp();
        dirA = root / "A";
        dirB = root / "B";
        std::filesystem::create_directories(dirA);
        std::filesystem::create_directories(dirB);
    }
};

#endif // __linux__

} // namespace

#if defined(__linux__)

TEST_F(WatcherReconfigurationFixture, ChangingPathWhileEnabledArmsTheNewDirectoryAndRetiresTheOld) {
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    // Control: the watch really is live on A before anything is reconfigured. Without this a
    // later "no event from A" assertion would also pass against a watcher that never worked.
    touchNew(dirA / "control");
    ASSERT_TRUE(r.awaitName("control")) << "the watcher was never armed on the original directory";

    w.setPathProperty(dirB.string());
    EXPECT_EQ(w.getPathProperty(), dirB.string());

    touchNew(dirA / "afterOnA");   // must NOT be reported: A is no longer the watched directory
    touchNew(dirB / "afterOnB");   // must be reported: B is

    EXPECT_TRUE(r.awaitName("afterOnB"))
        << "the new directory was never armed — before the repair this timed out, because the "
           "watch stayed on the old directory forever";
    EXPECT_FALSE(r.sawName("afterOnA"))
        << "a stale watch on the old directory survived the path change";
    EXPECT_FALSE(r.handlerFailed());

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, TheReportedFullPathNamesAFileThatActuallyExists) {
    // The pre-repair symptom was not merely a stale watch: the event came from directory A and
    // the FullPath was built from directory B, so it named a path that existed nowhere.
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);
    w.setPathProperty(dirB.string());

    touchNew(dirB / "real");
    ASSERT_TRUE(r.awaitName("real"));

    const std::string reported = r.fullPathOf("real");
    EXPECT_EQ(reported, (dirB / "real").string());
    EXPECT_TRUE(std::filesystem::exists(reported))
        << "the watcher reported a FullPath that names no existing file: " << reported;

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, ARejectedPathChangeLeavesTheOldWatchLiveAndUntouched) {
    // Validation runs before any teardown, so a bad path costs the caller nothing.
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    EXPECT_THROW(w.setPathProperty(""), System::ArgumentException);
    EXPECT_THROW(w.setPathProperty((root / "does_not_exist").string()), System::ArgumentException);

    EXPECT_EQ(w.getPathProperty(), dirA.string());
    EXPECT_TRUE(w.getEnableRaisingEventsProperty());

    touchNew(dirA / "stillWatched");
    EXPECT_TRUE(r.awaitName("stillWatched"))
        << "a rejected path change tore down a watch it had no business touching";

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, ChangingPathWhileDisabledTakesEffectOnTheNextEnable) {
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);

    w.setPathProperty(dirB.string());          // disabled: nothing to re-arm
    EXPECT_FALSE(w.getEnableRaisingEventsProperty());

    w.setEnableRaisingEventsProperty(true);
    touchNew(dirA / "onA");
    touchNew(dirB / "onB");

    EXPECT_TRUE(r.awaitName("onB"));
    EXPECT_FALSE(r.sawName("onA"));

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, EnablingBeforeAPathIsSetStillArmsWhenThePathArrives) {
    // startWatchingIfPossible() deliberately tolerates EnableRaisingEvents being set before Path
    // (it returns quietly when no directory is configured). That ordering only actually watches
    // anything because the re-arm is gated on enabled_ rather than on a thread already running.
    WatchRecorder r;
    FileSystemWatcher w;
    subscribeAll(w, r);

    w.setEnableRaisingEventsProperty(true);
    ASSERT_TRUE(w.getPathProperty().empty());

    w.setPathProperty(dirA.string());
    touchNew(dirA / "late");

    EXPECT_TRUE(r.awaitName("late"))
        << "a watcher enabled before its Path was set stayed permanently inert";

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, SettingPathToItsCurrentValueIsANoOpAndTheWatchSurvives) {
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    w.setPathProperty(dirA.string());          // identical value: the early return
    EXPECT_TRUE(w.getEnableRaisingEventsProperty());

    touchNew(dirA / "survives");
    EXPECT_TRUE(r.awaitName("survives"));

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, RepeatedPathChangesLeakNoDescriptor) {
    // Plan §14: /proc/self/fd is the primary instrument for this module, not LSan. Each re-arm
    // creates a fresh inotify instance and eventfd, so a missing teardown would show up here.
    const int before = watcherFdCount();
    {
        FileSystemWatcher w(dirA.string());
        w.setEnableRaisingEventsProperty(true);
        for (int i = 0; i < 50; ++i) {
            w.setPathProperty((i % 2 ? dirB : dirA).string());
        }
        w.setEnableRaisingEventsProperty(false);
    }
    const int after = watcherFdCount();
    RecordProperty("fd_before", before);
    RecordProperty("fd_after", after);
    EXPECT_EQ(after, before) << "descriptor delta across 50 path re-arms";
}

TEST_F(WatcherReconfigurationFixture, APathChangeIsSafeWhileEventsAreStillArriving) {
    // The reconfiguration joins the watcher thread before writing directory_, which is what
    // makes this race-free; TSan proved it (3 reports before, 0 after). Here it is pinned as an
    // ordinary functional test: the watcher must still be usable after churn, not merely quiet.
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    for (int i = 0; i < 20; ++i) {
        touchNew(dirA / ("churnA" + std::to_string(i)));
        touchNew(dirB / ("churnB" + std::to_string(i)));
        appendByte(dirA / ("churnA" + std::to_string(i)));   // Changed as well as Created
        appendByte(dirB / ("churnB" + std::to_string(i)));
        w.setPathProperty((i % 2 ? dirB : dirA).string());
    }

    w.setPathProperty(dirB.string());
    touchNew(dirB / "settled");
    EXPECT_TRUE(r.awaitName("settled")) << "the watcher stopped working after repeated re-arms";
    EXPECT_FALSE(r.handlerFailed());

    w.setEnableRaisingEventsProperty(false);
}

#endif // __linux__

// ===========================================================================================
// #2345 / SR-AUD-346 class-crossing half, split from #2102 — the inotify mask was a constexpr
// constant, so NotifyFilter was validated, stored, and read by nothing.
//
// Before-state, measured over ALL TEN filter configurations × seven operations
// (build-probe/2102_probe2_before.log): every configuration produced the byte-identical seven
// events, including NotifyFilters(0). "A Size-only watcher raises Created" is therefore not an
// independent defect with its own mechanism — it is one cell of a table in which every cell was
// equal, and there was exactly one cause.
//
// What lands here is only the half that needs NO mapping policy: the public values fall into a
// NAME class (FileName, DirectoryName) and a CONTENT class (Attributes, Size, LastWrite,
// LastAccess, CreationTime, Security), and no value in one class can justify an event from the
// other. Allocating events WITHIN a class — IN_MODIFY to Size or LastWrite, IN_ATTRIB across
// five values, CreationTime which inotify cannot report at all, LastAccess which no mask serves,
// FileName versus DirectoryName which IN_ISDIR could separate — is ticket #2346 (needs_user).
// SR-AUD-346 therefore stays CONFIRMED; these tests pin only the class boundary.
// ===========================================================================================

#if defined(__linux__)

TEST_F(WatcherReconfigurationFixture, ASizeOnlyWatcherRaisesNoCreatedDeletedOrRenamed) {
    // The finding's own headline. The sentinel is an IN_MODIFY, which a Size-only mask still
    // admits, and inotify delivers in queue order within one instance — so once the sentinel
    // arrives, any Created/Deleted/Renamed the watcher was going to raise already has.
    touchNew(dirA / "sentinel");
    touchNew(dirA / "victim");
    touchNew(dirA / "renameMe");

    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    w.setNotifyFilterProperty(NotifyFilters::Size);
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    touchNew(dirA / "made");
    std::filesystem::remove(dirA / "victim");
    std::filesystem::rename(dirA / "renameMe", dirA / "renamed");
    appendByte(dirA / "sentinel");

    ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Changed, "sentinel"));
    EXPECT_FALSE(r.sawEvent(WatcherChangeTypes::Created, "made"))
        << "a Size-only watcher raised Created — the defect SR-AUD-346 names";
    EXPECT_FALSE(r.sawEvent(WatcherChangeTypes::Deleted, "victim"));
    EXPECT_FALSE(r.sawEvent(WatcherChangeTypes::Renamed, "renamed"));
    EXPECT_FALSE(r.handlerFailed());

    w.setEnableRaisingEventsProperty(false);
}

// ===========================================================================================
// #2346 / SR-AUD-346, the remainder — allocating events WITHIN a class
//
// #2345 landed the half that needed no policy. This is the half that does, and it is NOT
// derivable from the reference tree and never will be: NotifyFilters names the notifications
// Win32's ReadDirectoryChangesW produces, and inotify's event set is not a relabelling of it.
// The five questions are priced in docs/SystemIONamespaceReviewPlan.md, and the user answered
// them on 2026-08-17 as docs/StandingApprovals.md SA-7: 1(a), 2(a), 3(a), 4(c), 5(b).
//
// The shape of the answer is *permissive where Linux genuinely cannot discriminate,
// discriminating where it can*. The cases below are one per decision, and each is written so
// that the OPPOSITE decision would fail it.
// ===========================================================================================

TEST_F(WatcherReconfigurationFixture, Decision2a_IN_ATTRIB_ServesAllSixContentFilters) {
    // 2(a). IN_ATTRIB is one bit for chmod/chown/link-count/utimes and does not say which of
    // them happened, so every content-class value is served by it. This is the case that
    // guarantees no content filter is silently inert -- including CreationTime, which is
    // decision 3(a): inotify cannot report a btime change at all, so the content class is the
    // approximation it gets.
    const std::pair<const char*, NotifyFilters> contentFilters[] = {
        {"Attributes", NotifyFilters::Attributes}, {"Size", NotifyFilters::Size},
        {"LastWrite", NotifyFilters::LastWrite},   {"LastAccess", NotifyFilters::LastAccess},
        {"CreationTime", NotifyFilters::CreationTime}, {"Security", NotifyFilters::Security},
    };
    for (const auto& [label, filter] : contentFilters) {
        const std::filesystem::path dir = root / (std::string("attrib_") + label);
        std::filesystem::create_directories(dir);
        touchNew(dir / "subject");

        WatchRecorder r;
        FileSystemWatcher w(dir.string());
        w.setNotifyFilterProperty(filter);
        subscribeAll(w, r);
        w.setEnableRaisingEventsProperty(true);

        ASSERT_EQ(::chmod((dir / "subject").c_str(), 0600), 0);
        EXPECT_TRUE(r.awaitEvent(WatcherChangeTypes::Changed, "subject")) << label;
        w.setEnableRaisingEventsProperty(false);
    }
}

TEST_F(WatcherReconfigurationFixture, Decision1a_IN_MODIFY_ServesSizeAndLastWriteAndNothingElse) {
    // 1(a). A content write is reported to Size and to LastWrite -- Linux gives one bit and no
    // way to tell whether the length changed, so serving only one of the two would silently
    // remove behaviour from the other. It is NOT reported to the four values that describe
    // metadata rather than content.
    //
    // "written" and "sentinel" are separate files precisely so the two Changed events can be
    // told apart; a single subject would make the negative half unassertable.
    struct Row { const char* label; NotifyFilters filter; bool expectsWrite; };
    const Row rows[] = {
        {"Size",         NotifyFilters::Size,         true},
        {"LastWrite",    NotifyFilters::LastWrite,    true},
        {"Attributes",   NotifyFilters::Attributes,   false},
        {"CreationTime", NotifyFilters::CreationTime, false},
        {"Security",     NotifyFilters::Security,     false},
    };
    for (const auto& row : rows) {
        const std::filesystem::path dir = root / (std::string("modify_") + row.label);
        std::filesystem::create_directories(dir);
        touchNew(dir / "written");
        touchNew(dir / "sentinel");

        WatchRecorder r;
        FileSystemWatcher w(dir.string());
        w.setNotifyFilterProperty(row.filter);
        subscribeAll(w, r);
        w.setEnableRaisingEventsProperty(true);

        appendByte(dir / "written");
        // Every row admits IN_ATTRIB (decision 2(a)), so the chmod is a sentinel that arrives
        // for all five and orders the negative assertion: inotify delivers in queue order
        // within one instance, so once it lands, any write event was already going to be there.
        ASSERT_EQ(::chmod((dir / "sentinel").c_str(), 0600), 0);
        ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Changed, "sentinel")) << row.label;

        EXPECT_EQ(r.sawEvent(WatcherChangeTypes::Changed, "written"), row.expectsWrite)
            << row.label << ": IN_MODIFY serves Size and LastWrite only";
        w.setEnableRaisingEventsProperty(false);
    }
}

TEST_F(WatcherReconfigurationFixture, Decision4c_IN_ACCESS_ArrivesOnlyWhenLastAccessIsNamed) {
    // 4(c). Before #2346, IN_ACCESS was in no mask at all, so LastAccess was a named filter that
    // could not fire for its own operation. Adding it to the whole content class (option b)
    // would make every read wake every content watcher, which is a large volume change on a
    // busy directory; adding it only when LastAccess is named costs nothing to anyone else.
    {
        const std::filesystem::path dir = root / "access_named";
        std::filesystem::create_directories(dir);
        touchNew(dir / "readme");
        // The file must have content before the watcher is armed: a read() that returns zero
        // bytes at end-of-file produces no IN_ACCESS, so an empty subject would make this case
        // pass or fail for a reason that has nothing to do with the filter.
        appendByte(dir / "readme");

        WatchRecorder r;
        FileSystemWatcher w(dir.string());
        w.setNotifyFilterProperty(NotifyFilters::LastAccess);
        subscribeAll(w, r);
        w.setEnableRaisingEventsProperty(true);

        readByte(dir / "readme");
        EXPECT_TRUE(r.awaitEvent(WatcherChangeTypes::Changed, "readme"))
            << "a LastAccess watcher must fire for a read";
        w.setEnableRaisingEventsProperty(false);
    }
    {
        const std::filesystem::path dir = root / "access_unnamed";
        std::filesystem::create_directories(dir);
        touchNew(dir / "readme");
        appendByte(dir / "readme");     // as above: an empty file cannot produce IN_ACCESS
        touchNew(dir / "sentinel");

        WatchRecorder r;
        FileSystemWatcher w(dir.string());
        w.setNotifyFilterProperty(NotifyFilters::Size);
        subscribeAll(w, r);
        w.setEnableRaisingEventsProperty(true);

        readByte(dir / "readme");
        appendByte(dir / "sentinel");       // IN_MODIFY: admitted by Size, orders the negative
        ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Changed, "sentinel"));
        EXPECT_FALSE(r.sawEvent(WatcherChangeTypes::Changed, "readme"))
            << "a Size-only watcher must not be woken by every read";
        w.setEnableRaisingEventsProperty(false);
    }
}

TEST_F(WatcherReconfigurationFixture, Decision5b_FileNameAndDirectoryNameDiscriminateOnIN_ISDIR) {
    // 5(b). IN_ISDIR travels on the event, so this is the one decision that could not be made in
    // the subscription mask and had to be made in dispatch. The information exists and the two
    // filters are meant to differ, so before #2346 a FileName-only watcher reported subdirectory
    // creation and a DirectoryName-only watcher reported file creation.
    {
        const std::filesystem::path dir = root / "isdir_file";
        std::filesystem::create_directories(dir);

        WatchRecorder r;
        FileSystemWatcher w(dir.string());
        w.setNotifyFilterProperty(NotifyFilters::FileName);
        subscribeAll(w, r);
        w.setEnableRaisingEventsProperty(true);

        std::filesystem::create_directory(dir / "kidDir");
        touchNew(dir / "kidFile");
        ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "kidFile"));
        EXPECT_FALSE(r.sawEvent(WatcherChangeTypes::Created, "kidDir"))
            << "a FileName-only watcher reported a subdirectory";
        w.setEnableRaisingEventsProperty(false);
    }
    {
        const std::filesystem::path dir = root / "isdir_dir";
        std::filesystem::create_directories(dir);

        WatchRecorder r;
        FileSystemWatcher w(dir.string());
        w.setNotifyFilterProperty(NotifyFilters::DirectoryName);
        subscribeAll(w, r);
        w.setEnableRaisingEventsProperty(true);

        touchNew(dir / "kidFile");
        std::filesystem::create_directory(dir / "kidDir");
        ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "kidDir"));
        EXPECT_FALSE(r.sawEvent(WatcherChangeTypes::Created, "kidFile"))
            << "a DirectoryName-only watcher reported a file";
        w.setEnableRaisingEventsProperty(false);
    }
}

TEST_F(WatcherReconfigurationFixture, Decision5b_AppliesToDeletedAndToBothHalvesOfARename) {
    // The discrimination has to reach every name-class event, not only Created -- and a rename
    // is the awkward one, because it is assembled from two inotify events and its IN_ISDIR
    // arrives on both. A watcher naming only DirectoryName must report the directory rename and
    // neither half of the file rename.
    const std::filesystem::path dir = root / "isdir_rename";
    std::filesystem::create_directories(dir);
    touchNew(dir / "oldFile");
    std::filesystem::create_directory(dir / "oldDir");
    touchNew(dir / "doomedFile");
    std::filesystem::create_directory(dir / "doomedDir");

    WatchRecorder r;
    FileSystemWatcher w(dir.string());
    w.setNotifyFilterProperty(NotifyFilters::DirectoryName);
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    std::filesystem::rename(dir / "oldFile", dir / "newFile");
    std::filesystem::remove(dir / "doomedFile");
    std::filesystem::rename(dir / "oldDir", dir / "newDir");
    std::filesystem::remove(dir / "doomedDir");

    ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Deleted, "doomedDir"));
    EXPECT_TRUE(r.sawEvent(WatcherChangeTypes::Renamed, "newDir"));
    EXPECT_FALSE(r.sawEvent(WatcherChangeTypes::Renamed, "newFile"))
        << "a DirectoryName-only watcher reported a file rename";
    EXPECT_FALSE(r.sawEvent(WatcherChangeTypes::Deleted, "doomedFile"))
        << "a DirectoryName-only watcher reported a file deletion";
    EXPECT_FALSE(r.handlerFailed());

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, ANameOnlyWatcherRaisesNoChanged) {
    // The mirror image of the finding's headline, and unambiguous for exactly the same reason:
    // FileName and DirectoryName describe directory entries, not file content or metadata.
    touchNew(dirA / "subject");

    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    w.setNotifyFilterProperty(NotifyFilters::FileName);
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    appendByte(dirA / "subject");
    ASSERT_EQ(::chmod((dirA / "subject").c_str(), 0600), 0);
    touchNew(dirA / "sentinel");                      // IN_CREATE: in a name-class mask

    ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "sentinel"));
    EXPECT_FALSE(r.sawEvent(WatcherChangeTypes::Changed, "subject"))
        << "a FileName-only watcher raised Changed";

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, TheDefaultFilterIsCompletelyUNAFFECTED) {
    // The default is LastWrite|FileName|DirectoryName, which spans BOTH classes, so it must see
    // exactly what it saw before this repair. Every existing end-to-end watcher test relies on
    // this, and none of them sets a filter at all.
    touchNew(dirA / "subject");
    touchNew(dirA / "attrib");
    touchNew(dirA / "victim");
    touchNew(dirA / "renameMe");

    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    touchNew(dirA / "made");
    appendByte(dirA / "subject");
    ASSERT_EQ(::chmod((dirA / "attrib").c_str(), 0600), 0);
    std::filesystem::remove(dirA / "victim");
    std::filesystem::rename(dirA / "renameMe", dirA / "renamed");
    touchNew(dirA / "sentinel");

    ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "sentinel"));
    EXPECT_TRUE(r.sawEvent(WatcherChangeTypes::Created, "made"));
    EXPECT_TRUE(r.sawEvent(WatcherChangeTypes::Changed, "subject"));
    EXPECT_TRUE(r.sawEvent(WatcherChangeTypes::Changed, "attrib"))
        << "IN_ATTRIB no longer reaches Changed under the default filter";
    EXPECT_TRUE(r.sawEvent(WatcherChangeTypes::Deleted, "victim"));
    EXPECT_TRUE(r.sawEvent(WatcherChangeTypes::Renamed, "renamed"));

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, NotifyFiltersZeroNamesNoChangeAndAdmitsNothing) {
    // NotifyFilters(0) passes validation (it has no bit outside the valid mask) and names no
    // change to watch. It is the ONE case with no sentinel available, so the deadline below is
    // load-bearing and is called out as such rather than dressed up as an event wait.
    touchNew(dirA / "subject");

    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    w.setNotifyFilterProperty(static_cast<NotifyFilters>(0));
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    touchNew(dirA / "made");
    appendByte(dirA / "subject");

    EXPECT_FALSE(r.awaitName("made", std::chrono::milliseconds(750)));
    EXPECT_EQ(r.size(), 0u) << "a filter naming no change still admitted events";
    EXPECT_TRUE(w.getEnableRaisingEventsProperty())
        << "an empty filter must be inert, not an error";

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, ChangingNotifyFilterWhileEnabledTakesEffect) {
    // The mask is fixed when the watch is armed, so the property is only genuinely consulted if
    // changing it rebuilds the watch.
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    w.setNotifyFilterProperty(NotifyFilters::Size);
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    w.setNotifyFilterProperty(NotifyFilters::FileName);
    EXPECT_TRUE(w.getEnableRaisingEventsProperty());

    touchNew(dirA / "afterWiden");
    EXPECT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "afterWiden"))
        << "NotifyFilter changed on a live watcher never reached the kernel-side mask";

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, SettingNotifyFilterToItsCurrentValueIsANoOp) {
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    w.setNotifyFilterProperty(NotifyFilters::LastWrite | NotifyFilters::FileName |
                              NotifyFilters::DirectoryName);   // the default, unchanged

    touchNew(dirA / "survives");
    EXPECT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "survives"));

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, AnInvalidNotifyFilterIsRejectedBeforeAnythingIsTornDown) {
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    EXPECT_THROW(w.setNotifyFilterProperty(static_cast<NotifyFilters>(0x40000)),
                 System::ArgumentException);

    touchNew(dirA / "stillWatched");
    EXPECT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "stillWatched"))
        << "a rejected NotifyFilter tore down a watch it had no business touching";

    w.setEnableRaisingEventsProperty(false);
}

#endif // __linux__

// ===========================================================================================
// #2104 — the pins. This ticket changes NO production behaviour; it makes the doc-comments true
// and pins what a future resolution of a DEFERRED or BLOCKED item would silently change.
//
// Three subjects, each pinned for a different reason:
//
//   (1) Plan §6.2's second measured POSITIVE. §6.2 recorded two: no descriptor is leaked over a
//       double Close(), and none over a THROWING constructor. #2099 pinned the first (100
//       cycles, above). The second was measured and left unpinned — so it is pinned here, and
//       plan §14 requires the number to be REPORTED, not merely asserted.
//
//   (2) #2105 (deferred) — whether a handler can be invoked after EnableRaisingEvents = false
//       RETURNS. What is pinned below is the OBSERVABLE behaviour only. #2105's own question is
//       NOT answered here and must not be read into these tests: it asks about a handler already
//       EXECUTING when the setter is called, which needs TSan plus a blocking-handler harness,
//       and its acceptance criteria says so.
//
//   (3) #2106 (deferred) — BinaryData decodes invalid UTF-8 as raw bytes (SR-AUD-185) and COPIES
//       where .NET's ReadOnlyMemory overload wraps (SR-AUD-186). Plan §6.1 records that
//       SR-AUD-186's premise is INVERTED: .NET's behaviour is the aliasing one and the port's is
//       the defensive one, so "fixing" it means making BinaryData alias caller memory it does not
//       own. Neither is decidable with the reference tree absent. Both are pinned as they stand.
// ===========================================================================================

TEST_F(IoReviewFixture, AThrowingFileStreamConstructorLeaksNoDescriptor) {
    // Plan §6.2, second positive: 20 constructors that throw (FileMode::Open on a missing file)
    // moved /proc/self/fd by 0. Pinned at 100 rather than 20 — a leak of one descriptor per
    // construction would be unmissable at either count, and the larger number costs nothing.
    auto fdCount = [] {
        int n = 0; std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator("/proc/self/fd", ec)) { (void)e; ++n; }
        return n;
    };
    const std::string missing = under("no-such-file.txt");
    ASSERT_FALSE(std::filesystem::exists(missing));

    const int before = fdCount();
    for (int i = 0; i < 100; ++i) {
        EXPECT_THROW(FileStream(missing, FileMode::Open, FileAccess::Read),
                     System::IO::FileNotFoundException);
    }
    const int after = fdCount();
    RecordProperty("fd_before", before);
    RecordProperty("fd_after", after);
    EXPECT_EQ(after, before) << "descriptor delta across 100 throwing FileStream constructors";

    // The file must not have been created on the way out: FileMode::Open does not create, and a
    // rejection that left a file behind would be a different defect wearing the same symptom.
    EXPECT_FALSE(std::filesystem::exists(missing));
}

#if defined(__linux__)

TEST_F(WatcherReconfigurationFixture, NoHandlerRunsForActivityAfterEnableRaisingEventsGoesFalse) {
    // #2105 PIN — the OBSERVABLE half only. Deterministic: after the setter returns, activity in
    // the watched directory is followed by a RE-ENABLED sentinel. Once the sentinel arrives the
    // watcher is demonstrably live again, so anything the disabled window was going to report
    // would already have been reported. No sleep is used as synchronisation.
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);

    touchNew(dirA / "whileEnabled");
    ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "whileEnabled"))
        << "the watcher was never live, so the rest of this test would prove nothing";

    w.setEnableRaisingEventsProperty(false);

    // Everything below happens with the watcher off.
    touchNew(dirA / "whileDisabled1");
    touchNew(dirA / "whileDisabled2");
    std::filesystem::remove(dirA / "whileDisabled1");

    w.setEnableRaisingEventsProperty(true);
    touchNew(dirA / "sentinelAfterReEnable");
    ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "sentinelAfterReEnable"));

    EXPECT_FALSE(r.sawName("whileDisabled1"))
        << "activity during a disabled window was reported — #2105's OBSERVABLE half changed";
    EXPECT_FALSE(r.sawName("whileDisabled2"));
    EXPECT_FALSE(r.handlerFailed());

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, DisablingIsIdempotentAndTheWatcherStaysUsableAfterwards) {
    // The second observable half: disabling twice is safe, and the watcher is not left in a state
    // that cannot be re-armed. Pinned because #2105's eventual answer could plausibly change the
    // disable path (a drain, a flag, a second thread), and any of those would show up here.
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);

    w.setEnableRaisingEventsProperty(true);
    w.setEnableRaisingEventsProperty(false);
    w.setEnableRaisingEventsProperty(false);
    EXPECT_FALSE(w.getEnableRaisingEventsProperty());

    w.setEnableRaisingEventsProperty(true);
    touchNew(dirA / "afterTheSecondEnable");
    EXPECT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "afterTheSecondEnable"));
    EXPECT_FALSE(r.handlerFailed());

    w.setEnableRaisingEventsProperty(false);
}

// --- Ticket #2347: reconfiguring from inside a handler ------------------------------------
//
// Handlers run ON the watcher thread. All three reconfiguring members routed through
// stopWatchingIfRunning(), which called watchThread_.join() unconditionally -- so calling any of
// them from a handler was a self-join, raising std::system_error("Resource deadlock avoided",
// code 35). watchLoop invoked handlers with NO try/catch, so with the handler not wrapping the
// call -- which is what a ported caller writes -- that exception reached std::terminate: measured
// SIGABRT, exit 134 (build-probe/2104_probe1_modeA.log). Every test below crashed the whole
// executable before the repair, which is why they assert on a flag set AFTER the call returns
// rather than merely on the absence of an exception.

TEST_F(WatcherReconfigurationFixture, DisablingFromInsideItsOwnHandlerReturnsInsteadOfTerminating) {
    // .NET permits a handler to stop its own watcher, so this is a permitted call, not a rejected
    // one: the stop is signalled and the thread is reaped later instead of being joined here.
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);

    std::atomic<bool> setterReturned{false};
    std::atomic<bool> threw{false};
    w.Created.push_back([&](void*, const FileSystemEventArgs&) {
        try {
            w.setEnableRaisingEventsProperty(false);
            setterReturned.store(true);
        } catch (...) {
            threw.store(true);
        }
    });

    w.setEnableRaisingEventsProperty(true);
    touchNew(dirA / "selfStop");
    ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "selfStop"));

    // Waiting on the recorder proves the handler ran; this proves it got PAST the setter rather
    // than dying inside it.
    for (int i = 0; i < 2000 && !setterReturned.load() && !threw.load(); ++i)
        std::this_thread::yield();
    EXPECT_TRUE(setterReturned.load()) << "the setter never returned to the handler that called it";
    EXPECT_FALSE(threw.load()) << "the self-join exception is still raised";
    EXPECT_FALSE(w.getEnableRaisingEventsProperty());
}

TEST_F(WatcherReconfigurationFixture, AWatcherDisabledFromItsOwnHandlerCanStillBeReEnabled) {
    // The self-stopped thread stays joinable until an external caller reaps it, so without
    // reapSelfStoppedThread() the arming path would mistake it for a live watch and return
    // quietly -- leaving the watcher enabled and permanently inert.
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);

    std::atomic<int> stops{0};
    w.Created.push_back([&](void*, const FileSystemEventArgs&) {
        if (stops.load() == 0) {
            w.setEnableRaisingEventsProperty(false);
            stops.fetch_add(1);
        }
    });

    w.setEnableRaisingEventsProperty(true);
    touchNew(dirA / "first");
    ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "first"));
    awaitCounter(stops);

    w.setEnableRaisingEventsProperty(true); // reaps the self-stopped thread, then arms afresh
    EXPECT_TRUE(w.getEnableRaisingEventsProperty());
    touchNew(dirA / "afterReEnable");
    EXPECT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "afterReEnable"))
        << "the watcher was never re-armed after a handler stopped it";

    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, SettingPathFromInsideAHandlerThrowsAndChangesNothing) {
    // Rejected rather than deferred: re-arming retires the inotify watch this very thread is
    // dispatching from. The watcher's state must survive the rejection untouched.
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);

    std::atomic<bool> sawInvalidOperation{false};
    std::atomic<bool> sawSomethingElse{false};
    w.Created.push_back([&](void*, const FileSystemEventArgs&) {
        try {
            w.setPathProperty(dirB.string());
            sawSomethingElse.store(true); // returned normally: also a failure
        } catch (const System::InvalidOperationException&) {
            sawInvalidOperation.store(true);
        } catch (...) {
            sawSomethingElse.store(true);
        }
    });

    w.setEnableRaisingEventsProperty(true);
    touchNew(dirA / "pathProbe");
    ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "pathProbe"));
    for (int i = 0; i < 2000 && !sawInvalidOperation.load() && !sawSomethingElse.load(); ++i)
        std::this_thread::yield();

    EXPECT_TRUE(sawInvalidOperation.load())
        << "Path= from a handler did not report InvalidOperationException";
    EXPECT_FALSE(sawSomethingElse.load());
    EXPECT_EQ(w.getPathProperty(), dirA.string()) << "the rejected change was partly applied";

    // Still live on the original directory: the rejection tore nothing down.
    touchNew(dirA / "stillWatching");
    EXPECT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "stillWatching"));
    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, SettingNotifyFilterFromInsideAHandlerThrowsAndChangesNothing) {
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);
    const NotifyFilters before = w.getNotifyFilterProperty();

    std::atomic<bool> sawInvalidOperation{false};
    w.Created.push_back([&](void*, const FileSystemEventArgs&) {
        try {
            w.setNotifyFilterProperty(NotifyFilters::Size);
        } catch (const System::InvalidOperationException&) {
            sawInvalidOperation.store(true);
        } catch (...) {
        }
    });

    w.setEnableRaisingEventsProperty(true);
    touchNew(dirA / "filterProbe");
    ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "filterProbe"));
    for (int i = 0; i < 2000 && !sawInvalidOperation.load(); ++i) std::this_thread::yield();

    EXPECT_TRUE(sawInvalidOperation.load());
    EXPECT_EQ(w.getNotifyFilterProperty(), before);
    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, ReconfiguringFromAnotherThreadIsUnaffected) {
    // The control the rejections need: the thread-identity check must reject ONLY the watcher's
    // own thread. A different thread reconfigures exactly as it always did.
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);
    w.setEnableRaisingEventsProperty(true);
    touchNew(dirA / "control");
    ASSERT_TRUE(r.awaitName("control"));

    std::thread other([&] {
        w.setPathProperty(dirB.string());
        w.setNotifyFilterProperty(NotifyFilters::FileName | NotifyFilters::LastWrite);
    });
    other.join();

    EXPECT_EQ(w.getPathProperty(), dirB.string());
    touchNew(dirB / "afterOtherThread");
    EXPECT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "afterOtherThread"));
    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, AnExceptionEscapingAHandlerReachesErrorInsteadOfTerminating) {
    // The second half of the same mechanism: handler invocation had no try/catch at all, so ANY
    // exception escaping ANY handler -- not only the self-join -- ended the process.
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);

    std::atomic<int> errorsSeen{0};
    w.Error.push_back([&](void*, const System::IO::ErrorEventArgs&) { errorsSeen.fetch_add(1); });
    w.Created.push_back([](void*, const FileSystemEventArgs&) {
        throw System::InvalidOperationException("a handler that throws");
    });

    w.setEnableRaisingEventsProperty(true);
    touchNew(dirA / "throwingHandler");
    ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "throwingHandler"));
    awaitCounter(errorsSeen);

    EXPECT_GE(errorsSeen.load(), 1) << "the escaped exception was not delivered to Error";

    // The watch survives a throwing handler rather than dying with it.
    touchNew(dirA / "afterTheThrow");
    EXPECT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "afterTheThrow"));
    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, AnErrorHandlerThatThrowsIsSwallowedRatherThanRecursing) {
    WatchRecorder r;
    FileSystemWatcher w(dirA.string());
    subscribeAll(w, r);

    std::atomic<int> errorCalls{0};
    w.Error.push_back([&](void*, const System::IO::ErrorEventArgs&) {
        errorCalls.fetch_add(1);
        throw System::InvalidOperationException("an Error handler that throws");
    });
    w.Created.push_back([](void*, const FileSystemEventArgs&) {
        throw System::InvalidOperationException("a handler that throws");
    });

    w.setEnableRaisingEventsProperty(true);
    touchNew(dirA / "doubleThrow");
    ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "doubleThrow"));
    awaitCounter(errorCalls);
    EXPECT_EQ(errorCalls.load(), 1) << "a throwing Error handler was re-entered";
    w.setEnableRaisingEventsProperty(false);
}

TEST_F(WatcherReconfigurationFixture, DestroyingAWatcherThatStoppedItselfDoesNotHangOrCrash) {
    // The destructor runs on a thread that is NOT the watcher thread, so it must still join and
    // close the descriptors the self-stop deliberately left open.
    WatchRecorder r;
    {
        FileSystemWatcher w(dirA.string());
        subscribeAll(w, r);
        w.Created.push_back([&](void*, const FileSystemEventArgs&) {
            w.setEnableRaisingEventsProperty(false);
        });
        w.setEnableRaisingEventsProperty(true);
        touchNew(dirA / "stopThenDestroy");
        ASSERT_TRUE(r.awaitEvent(WatcherChangeTypes::Created, "stopThenDestroy"));
    }
    SUCCEED() << "the watcher was destroyed after stopping itself";
}

#endif // __linux__

// -------------------------------------------------------------------------------------------
// #2106 PIN — BinaryData. Both findings are DEFERRED, so what is pinned is the CURRENT answer,
// stated in both directions: what the port does, and what the finding says .NET does. A future
// resolution has to edit these tests, which is exactly the point.
// -------------------------------------------------------------------------------------------

TEST(BinaryDataDeferredBehaviourPins, ToStringReturnsInvalidUtf8BytesUNCHANGED) {
    // SR-AUD-185: ToString() of 0xFF returns FF. .NET's UTF-8 decoder substitutes U+FFFD
    // (EF BF BD). The port does no validation at all — ToString is a byte-range copy.
    const std::vector<uint8_t> invalid{0xFF};
    const System::BinaryData bd(invalid);
    const std::string text = bd.ToString();

    ASSERT_EQ(text.size(), 1u) << "a substituting decoder would produce 3 bytes, not 1";
    EXPECT_EQ(static_cast<unsigned char>(text[0]), 0xFFu);
    EXPECT_NE(text, std::string("\xEF\xBF\xBD"))
        << "U+FFFD substitution landed without #2106 being resolved";
}

TEST(BinaryDataDeferredBehaviourPins, ATruncatedMultiByteSequenceIsAlsoPassedThroughUnchanged) {
    // Widens the pin past the finding's single byte: a lead byte with no continuation is the
    // other shape a decoder would have to substitute for, and it is equally untouched here.
    const std::vector<uint8_t> truncated{0xE2, 0x82};   // first two bytes of U+20AC
    const System::BinaryData bd(truncated);
    const std::string text = bd.ToString();

    ASSERT_EQ(text.size(), 2u);
    EXPECT_EQ(static_cast<unsigned char>(text[0]), 0xE2u);
    EXPECT_EQ(static_cast<unsigned char>(text[1]), 0x82u);
}

TEST(BinaryDataDeferredBehaviourPins, ValidUtf8IsUnaffectedEitherWay) {
    // The control. Every case the audit could reach directly used valid ASCII, which is why the
    // finding could sit undetected; a resolution of #2106 must not disturb this row.
    const System::BinaryData bd(std::string("hello"));
    EXPECT_EQ(bd.ToString(), "hello");
}

TEST(BinaryDataDeferredBehaviourPins, EveryConstructionPathCOPIESItsSource) {
    // SR-AUD-186, and plan §6.1's INVERTED premise: .NET's ReadOnlyMemory overload WRAPS and
    // observes the caller's later writes; this port copies and does not. Pinned across all four
    // doors, because "fixing" this means making BinaryData alias memory it does not own — a
    // borrowed-pointer lifetime hazard of exactly the CCF-019 shape, in a type that has none.
    std::vector<uint8_t> source{0x01};

    const System::BinaryData fromVectorCtor(source);
    const System::BinaryData fromVectorFactory = System::BinaryData::FromBytes(source);
    const System::ReadOnlyMemory<uint8_t> view(source.data(), 1);
    const System::BinaryData fromMemoryCtor{view};
    const System::BinaryData fromMemoryFactory = System::BinaryData::FromBytes(view);

    source[0] = 0x02;   // the caller mutates the source AFTER construction

    EXPECT_EQ(fromVectorCtor[0], 0x01) << "the vector constructor started aliasing its source";
    EXPECT_EQ(fromVectorFactory[0], 0x01);
    EXPECT_EQ(fromMemoryCtor[0], 0x01)
        << "the ReadOnlyMemory constructor started WRAPPING — #2106 resolved without a decision";
    EXPECT_EQ(fromMemoryFactory[0], 0x01)
        << "FromBytes(ReadOnlyMemory) started WRAPPING — #2106 resolved without a decision";
}

TEST(BinaryDataDeferredBehaviourPins, TheCopyOutlivesASourceThatIsGoneEntirely) {
    // The consequence that makes the current behaviour the DEFENSIVE one, and the reason §6.1
    // says the finding's direction is inverted: a wrapping BinaryData built this way would be
    // reading freed memory here.
    System::BinaryData bd = System::BinaryData::FromBytes(std::vector<uint8_t>{});
    {
        std::vector<uint8_t> shortLived{0x07, 0x08};
        bd = System::BinaryData::FromBytes(
            System::ReadOnlyMemory<uint8_t>(shortLived.data(), 2));
    }
    ASSERT_EQ(bd.getLengthProperty(), 2);
    EXPECT_EQ(bd[0], 0x07);
    EXPECT_EQ(bd[1], 0x08);
}
