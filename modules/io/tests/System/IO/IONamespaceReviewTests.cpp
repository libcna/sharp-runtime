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
//   #2100 / SR-AUD-340, cause I-B — RandomAccess::Write accepted a negative count SILENTLY, and
//          every other rejection in the class was untyped: a bare IOException that named neither
//          the offending parameter nor the native reason. GetLength returned the sentinel -1.
//
// Reference tree absent: every exception type and paramName asserted here is recorded as this
// port's choice in the plan, not as a verified match to .NET.
#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

#include <filesystem>
#include <string>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IO/Directory.hpp"
#include "System/IO/DirectoryInfo.hpp"
#include "System/IO/File.hpp"
#include "System/IO/FileInfo.hpp"
#include "System/IO/IOException.hpp"
#include "System/IO/RandomAccess.hpp"

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
