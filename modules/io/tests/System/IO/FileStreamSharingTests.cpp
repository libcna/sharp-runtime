// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
// CNA-DEMOS-ISSUE-152: native sharing must be implemented on the actual stream handle.
#include <gtest/gtest.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <type_traits>
#include <cstdlib>
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#include "System/IO/FileStream.hpp"
#include "System/IO/FileShare.hpp"
#include "System/IO/File.hpp"
#include "System/IO/IOException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace
{
namespace fs = std::filesystem;
using namespace System::IO;

TEST(FileStreamSharingSurfaceTest, TheNativeShareConstructorIsAvailable)
{
    EXPECT_TRUE((std::is_constructible_v<FileStream, const std::string&,
                                        FileMode, FileAccess, FileShare>));
}

class FileStreamSharingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        root_ = fs::temp_directory_path() /
            ("SharpFileShare-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(fs::create_directory(root_));
        owned_ = true;
        file_ = root_ / "seed.bin";
        std::ofstream seed(file_, std::ios::binary);
        const char bytes[] = {1, 2, 3, 4};
        seed.write(bytes, sizeof(bytes));
        ASSERT_TRUE(seed.good());
    }
    void TearDown() override
    {
        if (owned_)
        {
            std::error_code error;
            fs::remove_all(root_, error);
        }
    }
    fs::path root_, file_;
    bool owned_ = false;
    std::string Path() const
    {
        const auto utf8 = file_.u8string();
        return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
    }
};

class FileStreamWritePreservationTest : public FileStreamSharingTest,
                                      public ::testing::WithParamInterface<FileMode> {};

TEST_P(FileStreamWritePreservationTest, WriteOnlyOpenPreservesLengthPositionAndUntouchedBytes)
{
    const auto utf8 = file_.u8string();
    const std::string path(reinterpret_cast<const char*>(utf8.data()), utf8.size());
    {
        FileStream stream(path, GetParam(), FileAccess::Write);
        EXPECT_EQ(stream.getLengthProperty(), 4);
        EXPECT_EQ(stream.getPositionProperty(), 0);
        stream.WriteByte(9);
    }
    std::array<char, 5> bytes{};
    std::ifstream reader(file_, std::ios::binary);
    reader.read(bytes.data(), bytes.size());
    EXPECT_EQ(reader.gcount(), 4);
    EXPECT_EQ(bytes[0], 9);
    EXPECT_EQ(bytes[1], 2);
    EXPECT_EQ(bytes[2], 3);
    EXPECT_EQ(bytes[3], 4);
}

INSTANTIATE_TEST_SUITE_P(ExistingFile, FileStreamWritePreservationTest,
                        ::testing::Values(FileMode::Open, FileMode::OpenOrCreate));

TEST(FileStreamSharingSurfaceTest, LayoutAndOwnershipBeforeAndAfterNativeHandle)
{
    // SA-3: reconstruct the old private storage, with the same polymorphic Stream base.
    // sizeof is valid even though this deliberately has no implementations of pure members.
    struct PreviousLayout : Stream {
        std::fstream file;
        std::string path;
        intcs length;
        bool canRead, canWrite;
    };
#if defined(__x86_64__) && defined(__GLIBCXX__) && !defined(_WIN32)
    EXPECT_EQ(sizeof(PreviousLayout), 576u);
    EXPECT_EQ(alignof(PreviousLayout), 8u);
    EXPECT_EQ(sizeof(FileStream), 24u);
    EXPECT_EQ(alignof(FileStream), 8u);
#endif
    EXPECT_FALSE(std::is_copy_constructible_v<FileStream>);
    EXPECT_FALSE(std::is_copy_assignable_v<FileStream>);
    EXPECT_FALSE(std::is_move_constructible_v<FileStream>);
    EXPECT_FALSE(std::is_move_assignable_v<FileStream>);
    EXPECT_TRUE(std::is_nothrow_destructible_v<FileStream>);
}

#if !defined(__EMSCRIPTEN__)
TEST_F(FileStreamSharingTest, NativeAccessShareMatrixMatchesMeasuredPlatformContract)
{
    int refused = 0, allowed = 0;
    for (const auto firstAccess : {FileAccess::Read, FileAccess::Write, FileAccess::ReadWrite})
    for (const auto firstShare : {FileShare::None, FileShare::Read, FileShare::Write, FileShare::ReadWrite})
    for (const auto nextAccess : {FileAccess::Read, FileAccess::Write, FileAccess::ReadWrite})
    for (const auto nextShare : {FileShare::None, FileShare::Read, FileShare::Write, FileShare::ReadWrite})
    {
        SCOPED_TRACE(::testing::Message() << static_cast<int>(firstAccess) << '/'
            << static_cast<int>(firstShare) << " -> " << static_cast<int>(nextAccess)
            << '/' << static_cast<int>(nextShare));
#if defined(_WIN32)
        const bool expected = (static_cast<int>(nextAccess) & ~static_cast<int>(firstShare)) != 0 ||
                              (static_cast<int>(firstAccess) & ~static_cast<int>(nextShare)) != 0;
#else
        const bool expected = firstShare == FileShare::None || nextShare == FileShare::None;
#endif
        FileStream first(Path(), FileMode::Open, firstAccess, firstShare);
        bool denied = false;
        try { FileStream next(Path(), FileMode::Open, nextAccess, nextShare); }
        catch (const IOException&) { denied = true; }
        EXPECT_EQ(denied, expected);
        if (denied) ++refused; else ++allowed;
    }
#if defined(_WIN32)
    EXPECT_EQ(refused, 119);
    EXPECT_EQ(allowed, 25);
#else
    EXPECT_EQ(refused, 63);
    EXPECT_EQ(allowed, 81);
#endif
}

TEST_F(FileStreamSharingTest, SharingConflictIsRejectedBeforeCreateOrTruncate)
{
    for (const auto mode : {FileMode::Create, FileMode::Truncate})
    {
        FileStream owner(Path(), FileMode::Open, FileAccess::Read, FileShare::None);
        EXPECT_THROW(FileStream(Path(), mode, FileAccess::Write, FileShare::ReadWrite), IOException);
        EXPECT_EQ(owner.getLengthProperty(), 4);
        std::array<bytecs, 5> bytes{};
        EXPECT_EQ(owner.Read(bytes.data(), 0, 5), 4);
        EXPECT_EQ(bytes, (std::array<bytecs, 5>{1, 2, 3, 4, 0}));
    }
}

TEST_F(FileStreamSharingTest, AliasSharesTheHandleAndCloseReleasesIt)
{
    fs::create_directory(root_ / "child");
    const auto alias = root_ / "child" / ".." / "seed.bin";
    const auto utf8 = alias.u8string();
    const std::string aliasPath(reinterpret_cast<const char*>(utf8.data()), utf8.size());
    FileStream owner(Path(), FileMode::Open, FileAccess::Read, FileShare::None);
    EXPECT_THROW(FileStream(aliasPath, FileMode::Open, FileAccess::Read, FileShare::ReadWrite), IOException);
    owner.Close();
    owner.Close();
    EXPECT_NO_THROW(FileStream(aliasPath, FileMode::Open, FileAccess::Read, FileShare::None));
}

TEST_F(FileStreamSharingTest, FileCreateIsExclusiveButShortStreamConstructorsShareRead)
{
    {
        auto created = File::Create(Path());
        EXPECT_THROW(FileStream(Path(), FileMode::Open, FileAccess::Read, FileShare::ReadWrite), IOException);
    }
    {
        FileStream stream(Path(), FileMode::Open, FileAccess::Read);
        EXPECT_NO_THROW(FileStream(Path(), FileMode::Open, FileAccess::Read, FileShare::Read));
        EXPECT_THROW(FileStream(Path(), FileMode::Open, FileAccess::Read, FileShare::None), IOException);
    }
    {
        FileStream stream(Path());
        EXPECT_NO_THROW(FileStream(Path(), FileMode::Open, FileAccess::Read, FileShare::ReadWrite));
    }
}

TEST_F(FileStreamSharingTest, LengthAndResizeFollowTheHandleAfterRename)
{
    FileStream stream(Path(), FileMode::Open, FileAccess::ReadWrite, FileShare::ReadWrite | FileShare::Delete);
    fs::rename(file_, root_ / "moved.bin");
    { std::ofstream replacement(file_); replacement << "unrelated"; }
    EXPECT_EQ(stream.getLengthProperty(), 4);
    stream.SetLength(2);
    EXPECT_EQ(stream.getLengthProperty(), 2);
    EXPECT_EQ(fs::file_size(root_ / "moved.bin"), 2u);
    EXPECT_EQ(fs::file_size(file_), 9u);
}
#endif

TEST_F(FileStreamSharingTest, BufferedReadWriteTransitionsAndEofKeepLogicalPosition)
{
    FileStream stream(Path(), FileMode::Open, FileAccess::ReadWrite);
    EXPECT_EQ(stream.ReadByte(), 1);
    stream.WriteByte(9);
    EXPECT_EQ(stream.getPositionProperty(), 2);
    EXPECT_EQ(stream.ReadByte(), 3);
    EXPECT_EQ(stream.ReadByte(), 4);
    EXPECT_EQ(stream.ReadByte(), -1);
    EXPECT_EQ(stream.ReadByte(), -1);
    EXPECT_EQ(stream.getPositionProperty(), 4);
    stream.WriteByte(5);
    EXPECT_EQ(stream.getLengthProperty(), 5);
    stream.setPositionProperty(0);
    std::array<bytecs, 7> bytes{};
    EXPECT_EQ(stream.Read(bytes.data(), 0, 7), 5);
    EXPECT_EQ(bytes, (std::array<bytecs, 7>{1, 9, 3, 4, 5, 0, 0}));
    stream.SetLength(3);
    EXPECT_EQ(stream.getPositionProperty(), 3);
    stream.SetLength(6);
    EXPECT_EQ(stream.getPositionProperty(), 3);
    EXPECT_EQ(stream.ReadByte(), 0);
}

TEST_F(FileStreamSharingTest, AppendProtectsOriginalBytesButCanRewriteItsOwnTail)
{
    FileStream stream(Path(), FileMode::Append);
    EXPECT_EQ(stream.getPositionProperty(), 4);
    EXPECT_THROW(stream.setPositionProperty(3), IOException);
    EXPECT_EQ(stream.getPositionProperty(), 4);
    EXPECT_THROW(stream.SetLength(3), IOException);
    stream.WriteByte(5);
    stream.WriteByte(6);
    stream.setPositionProperty(4);
    stream.WriteByte(7);
    EXPECT_EQ(stream.getLengthProperty(), 6);
    stream.Close();
    auto reader = File::OpenRead(Path());
    reader.setPositionProperty(4);
    EXPECT_EQ(reader.ReadByte(), 7);
    EXPECT_EQ(reader.ReadByte(), 6);
}

TEST_F(FileStreamSharingTest, InvalidSharingFailsWithoutChangingFile)
{
    EXPECT_THROW(FileStream(Path(), FileMode::Create, FileAccess::Write, static_cast<FileShare>(8)),
                 System::ArgumentOutOfRangeException);
    EXPECT_EQ(fs::file_size(file_), 4u);
}

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
TEST_F(FileStreamSharingTest, HardLinkAndSymlinkCannotBypassExclusiveSharing)
{
    const auto hardLink = root_ / "hard.bin";
    const auto symbolicLink = root_ / "symbolic.bin";
    fs::create_hard_link(file_, hardLink);
    fs::create_symlink(file_, symbolicLink);
    FileStream owner(Path(), FileMode::Open, FileAccess::Read, FileShare::None);
    EXPECT_THROW(FileStream(hardLink.string(), FileMode::Open, FileAccess::Read, FileShare::ReadWrite), IOException);
    EXPECT_THROW(FileStream(symbolicLink.string(), FileMode::Open, FileAccess::Read, FileShare::ReadWrite), IOException);
}

TEST_F(FileStreamSharingTest, WriteOnlyOpenDoesNotRequireReadPermissions)
{
    fs::permissions(file_, fs::perms::owner_write);
    FileStream stream(Path(), FileMode::Open, FileAccess::Write);
    EXPECT_FALSE(stream.getCanReadProperty());
    EXPECT_EQ(stream.getLengthProperty(), 4);
    EXPECT_NO_THROW(stream.WriteByte(9));
    EXPECT_NO_THROW(stream.Close());
}

TEST_F(FileStreamSharingTest, BufferedWriteErrorsAreReportedAndCloseStillReleasesHandle)
{
    // Linux's deterministic failing sink: no disk-full race or global resource limit change.
    FileStream stream("/dev/full", FileMode::OpenOrCreate, FileAccess::Write, FileShare::ReadWrite);
    stream.WriteByte(1);
    EXPECT_THROW(stream.Close(), IOException);
    EXPECT_FALSE(stream.IsOpen());
    EXPECT_NO_THROW(stream.Close());
    FileStream other("/dev/full", FileMode::OpenOrCreate, FileAccess::Write, FileShare::ReadWrite);
    other.WriteByte(1);
    EXPECT_THROW(other.Flush(), IOException);
}

TEST_F(FileStreamSharingTest, RejectedConstructorsDoNotLeakDescriptorsOrLocks)
{
    const auto descriptorCount = [] {
        std::size_t count = 0;
        for ([[maybe_unused]] const auto& entry : fs::directory_iterator("/proc/self/fd")) ++count;
        return count;
    };
    FileStream owner(Path(), FileMode::Open, FileAccess::Read, FileShare::None);
    const auto before = descriptorCount();
    for (int i = 0; i < 32; ++i)
        EXPECT_THROW(FileStream(Path(), FileMode::Create, FileAccess::Write, FileShare::None), IOException);
    EXPECT_EQ(descriptorCount(), before);
    owner.Close();
    EXPECT_NO_THROW(FileStream(Path(), FileMode::Open, FileAccess::Read, FileShare::None));
}

TEST_F(FileStreamSharingTest, ExclusiveSharingReachesAnotherProcess)
{
    const std::string path = Path();
    FileStream owner(path, FileMode::Open, FileAccess::Read, FileShare::None);
    EXPECT_EXIT({
        try { FileStream child(path, FileMode::Open, FileAccess::Read, FileShare::ReadWrite); }
        catch (const IOException&) {
            // A fork also copies any hypothetical process-local reservation map. Probe the
            // kernel directly, without FileStream, so such a map cannot fake this regression.
            const int descriptor = open(path.c_str(), O_RDONLY | O_CLOEXEC);
            if (descriptor < 0) std::_Exit(3);
            const int result = flock(descriptor, LOCK_SH | LOCK_NB);
            const int error = errno;
            close(descriptor);
            std::_Exit(result < 0 && (error == EWOULDBLOCK || error == EAGAIN) ? 17 : 4);
        }
        std::_Exit(0);
    }, ::testing::ExitedWithCode(17), "");
    owner.Close();
    EXPECT_EXIT({
        try { FileStream child(path, FileMode::Open, FileAccess::Read, FileShare::None); }
        catch (...) { std::_Exit(1); }
        std::_Exit(0);
    }, ::testing::ExitedWithCode(0), "");
}
#endif
}
