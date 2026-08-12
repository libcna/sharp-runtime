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

#include <sstream>

#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/IO/Directory.hpp"
#include "System/IO/DirectoryInfo.hpp"
#include "System/IO/File.hpp"
#include "System/IO/FileInfo.hpp"
#include "System/IO/FileStream.hpp"
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
struct ShadowStringReader : ShadowTextReader { std::string s_; SharpRuntime::intcs pos_ = 0; };
struct ShadowStreamReader : ShadowTextReader {
    Stream* stream_; bool leaveOpen_; bool ownsStream_; bool hasPeeked_; SharpRuntime::bytecs peeked_;
};
struct ShadowStreamWriter : ShadowTextWriter { Stream* stream_; bool leaveOpen_; bool ownsStream_; };
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
    // StringWriter is the ONE type Approval IO-1 says will grow. Its size is dominated by
    // std::ostringstream, which is a standard-library implementation detail, so it is pinned as
    // a relationship rather than a literal: it is the base's vtable pointer plus the stream.
    EXPECT_EQ(sizeof(StringWriter), sizeof(void*) + sizeof(std::ostringstream));
    EXPECT_EQ(alignof(StringWriter), alignof(std::ostringstream));
}

TEST(IoLayoutPinTests, ThreeOfTheFourTextWrappersHaveSPARETAILPADDINGForApprovalIO1sFlag) {
    // Approval IO-1 claims a private bool costs StringReader, StreamReader and StreamWriter
    // NOTHING because it lands in existing tail padding. This asserts the padding is really
    // there, by measuring the shadow WITH the flag added.
    struct WithFlagStringReader : ShadowTextReader { std::string s_; SharpRuntime::intcs pos_ = 0; bool closed_ = false; };
    struct WithFlagStreamReader : ShadowTextReader {
        Stream* stream_; bool leaveOpen_; bool ownsStream_; bool hasPeeked_;
        SharpRuntime::bytecs peeked_; bool closed_ = false;
    };
    struct WithFlagStreamWriter : ShadowTextWriter { Stream* stream_; bool leaveOpen_; bool ownsStream_; bool closed_ = false; };
    EXPECT_EQ(sizeof(WithFlagStringReader), sizeof(ShadowStringReader))
        << "StringReader's flag was costed as free";
    EXPECT_EQ(sizeof(WithFlagStreamReader), sizeof(ShadowStreamReader))
        << "StreamReader's flag was costed as free";
    EXPECT_EQ(sizeof(WithFlagStreamWriter), sizeof(ShadowStreamWriter))
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
