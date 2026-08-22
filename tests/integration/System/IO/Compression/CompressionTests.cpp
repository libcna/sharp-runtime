// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <algorithm>
#include "System/NotImplementedException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Buffers/OperationStatus.hpp"
#include "System/IO/Compression/CompressionMode.hpp"
#include "System/IO/Compression/CompressionLevel.hpp"
#include "System/IO/Compression/GZipStream.hpp"
#include "System/IO/Compression/DeflateStream.hpp"
#include "System/IO/Compression/ZLibStream.hpp"
#include "System/IO/Compression/ZipArchive.hpp"
#include "System/IO/Compression/ZipFile.hpp"
#include "System/IO/Compression/ZipFileExtensions.hpp"
#include "System/IO/Compression/ZipCompressionMethod.hpp"
#include "System/IO/Directory.hpp"
#include "System/IO/DirectoryNotFoundException.hpp"
#include "System/IO/File.hpp"
#include "System/IO/IOException.hpp"
#include "System/IO/InvalidDataException.hpp"
#include "System/IO/Path.hpp"
#include <filesystem>
#include "System/IO/Compression/ZLibCompressionStrategy.hpp"
#include "System/IO/Compression/ZLibCompressionOptions.hpp"
#include "System/IO/Compression/ZLibException.hpp"
#include "System/IO/Compression/DeflateDecoder.hpp"
#include "System/IO/Compression/DeflateEncoder.hpp"
#include "System/IO/Compression/GZipDecoder.hpp"
#include "System/IO/Compression/GZipEncoder.hpp"
#include "System/IO/Compression/ZLibDecoder.hpp"
#include "System/IO/Compression/ZLibEncoder.hpp"
#include "System/IO/MemoryStream.hpp"
#include <miniz/miniz.h>
#include <vector>
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <stdexcept>
#include <utility>

using System::NotImplementedException;
using System::Buffers::OperationStatus;
using System::IO::Compression::CompressionMode;
using System::IO::Compression::CompressionLevel;
using System::IO::Compression::GZipStream;
using System::IO::Compression::DeflateStream;
using System::IO::Compression::ZLibStream;
using System::IO::Compression::ZipArchive;
using System::IO::Compression::ZipArchiveEntry;
using System::IO::Compression::ZipArchiveMode;
using System::IO::Compression::ZipFile;
using System::IO::Compression::ZipFileExtensions;
using System::IO::Directory;
using System::IO::File;
using System::IO::Path;
using System::IO::Compression::ZipCompressionMethod;
using System::IO::Compression::ZLibCompressionStrategy;
using System::IO::Compression::ZLibCompressionOptions;
using System::IO::Compression::ZLibException;
using System::IO::Compression::DeflateDecoder;
using System::IO::Compression::DeflateEncoder;
using System::IO::Compression::GZipDecoder;
using System::IO::Compression::GZipEncoder;
using System::IO::Compression::ZLibDecoder;
using System::IO::Compression::ZLibEncoder;

namespace {
class ScopedTemporaryDirectory final {
    std::filesystem::path path_;

public:
    ScopedTemporaryDirectory() {
        const auto parent = std::filesystem::temp_directory_path();
        static std::atomic<std::uint64_t> sequence{0};
        std::random_device entropy;
        for (int attempt = 0; attempt < 128; ++attempt) {
            const auto random =
                (static_cast<std::uint64_t>(entropy()) << 32U) ^ entropy();
            const auto suffix = random ^ sequence.fetch_add(1, std::memory_order_relaxed);
            auto candidate = parent / ("sharp-runtime-compression-" + std::to_string(suffix));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                path_ = std::move(candidate);
                return;
            }
        }
        throw std::runtime_error("could not create a unique compression-test directory");
    }

    ~ScopedTemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    ScopedTemporaryDirectory(const ScopedTemporaryDirectory&) = delete;
    ScopedTemporaryDirectory& operator=(const ScopedTemporaryDirectory&) = delete;

    [[nodiscard]] std::string path(const std::string& name) const {
        return (path_ / name).string();
    }
};

// Test double for the 2026-07-13 resource-management fix: verifies DeflateStream/GZipStream/
// ZLibStream::Close() cleans up zlib state (and doesn't hang/crash on a second Close() call, as
// the destructor performs) when the inner stream's Write() throws mid-flush.
class ThrowingWriteStream final : public System::IO::Stream {
public:
    System::IO::intcs Read(System::IO::bytecs*, System::IO::intcs, System::IO::intcs) override { return 0; }
    void Write(const System::IO::bytecs*, System::IO::intcs, System::IO::intcs) override {
        throw System::IO::IOException("ThrowingWriteStream: simulated write failure.");
    }
    void Close() override {}
    [[nodiscard]] System::IO::intcs getLengthProperty() const override { return 0; }
    // Ticket #1827: this stream IS writable -- it implements Write() -- it just throws to
    // simulate an I/O failure mid-flush. It previously omitted getCanWriteProperty() and so
    // inherited the base default of false, which the new ZipArchive(Create) capability guard
    // correctly rejected (docs/StreamCapabilityContractDesign.md §4: this is the one in-repository
    // migration the whole-gate experiment surfaced). Declaring the capability truthfully -- not
    // to bypass the guard -- lets the two Dispose/Destructor tests keep testing a *write* that
    // throws rather than a *construction* that is refused.
    [[nodiscard]] bool getCanWriteProperty() const override { return true; }
};
} // namespace
using System::IO::Stream;
using System::IO::MemoryStream;
using System::IO::intcs;

// ===========================================================================
// CompressionMode enum
// ===========================================================================

TEST(CompressionModeTests, Decompress_IsZero) {
    EXPECT_EQ(static_cast<int>(CompressionMode::Decompress), 0);
}

TEST(CompressionModeTests, Compress_IsOne) {
    EXPECT_EQ(static_cast<int>(CompressionMode::Compress), 1);
}

// ===========================================================================
// ZipArchiveMode enum
// ===========================================================================

TEST(ZipArchiveModeTests, Read_IsZero) {
    EXPECT_EQ(static_cast<int>(ZipArchiveMode::Read), 0);
}

TEST(ZipArchiveModeTests, Create_IsOne) {
    EXPECT_EQ(static_cast<int>(ZipArchiveMode::Create), 1);
}

TEST(ZipArchiveModeTests, Update_IsTwo) {
    EXPECT_EQ(static_cast<int>(ZipArchiveMode::Update), 2);
}

// ===========================================================================
// GZipStream
// ===========================================================================

TEST(GZipStreamTests, CompressMode_CanWrite_True) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_TRUE(gz.getCanWriteProperty());
}

TEST(GZipStreamTests, CompressMode_CanRead_False) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_FALSE(gz.getCanReadProperty());
}

TEST(GZipStreamTests, DecompressMode_CanRead_True) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    EXPECT_TRUE(gz.getCanReadProperty());
}

TEST(GZipStreamTests, DecompressMode_CanWrite_False) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    EXPECT_FALSE(gz.getCanWriteProperty());
}

TEST(GZipStreamTests, Read_EmptyStream_ReturnsZero) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {};
    EXPECT_EQ(gz.Read(buf, 0, 4), 0);
}

// Regression tests for audit finding A-03 (2026-07-14): Read() previously performed no argument
// validation at all, so a null buffer reached zlib's inflate() unchecked and a negative offset/
// count was silently swallowed by `count <= 0` instead of throwing, matching MemoryStream::Read
// and this class's own Write() validation.
TEST(GZipStreamTests, Read_NullBuffer_ThrowsArgumentNullException) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    EXPECT_THROW(gz.Read(nullptr, 0, 4), System::ArgumentNullException);
}

TEST(GZipStreamTests, Read_NegativeOffset_ThrowsArgumentOutOfRangeException) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {};
    EXPECT_THROW(gz.Read(buf, -1, 4), System::ArgumentOutOfRangeException);
}

TEST(GZipStreamTests, Read_NegativeCount_ThrowsArgumentOutOfRangeException) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {};
    EXPECT_THROW(gz.Read(buf, 0, -1), System::ArgumentOutOfRangeException);
}

TEST(GZipStreamTests, Write_DoesNotThrow) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    EXPECT_NO_THROW(gz.Write(buf, 0, 4));
}

// Post-stabilization audit ticket 1715: Write() previously only checked `count <= 0` -- no
// null-buffer or negative-offset check -- so a negative offset reached deflate()'s
// next_in = buffer + offset unchecked, an out-of-bounds read confirmed via a standalone ASan
// repro, not just a silent no-op.
TEST(GZipStreamTests, Write_NullBuffer_ThrowsArgumentNullException) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_THROW(gz.Write(nullptr, 0, 4), System::ArgumentNullException);
}

TEST(GZipStreamTests, Write_NegativeOffset_ThrowsArgumentOutOfRangeException) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    EXPECT_THROW(gz.Write(buf, -1, 2), System::ArgumentOutOfRangeException);
}

TEST(GZipStreamTests, Write_NegativeCount_ThrowsArgumentOutOfRangeException) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    EXPECT_THROW(gz.Write(buf, 0, -1), System::ArgumentOutOfRangeException);
}

TEST(GZipStreamTests, Flush_DoesNotThrow) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_NO_THROW(gz.Flush());
}

TEST(GZipStreamTests, Close_DoesNotThrow) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_NO_THROW(gz.Close());
}

TEST(GZipStreamTests, Close_InnerWriteThrows_PropagatesAndSecondCloseIsSafe) {
    ThrowingWriteStream inner;
    GZipStream gz(&inner, CompressionMode::Compress, /*leaveOpen=*/true);
    // Even with no prior explicit Write(), deflate(Z_FINISH) still emits gzip header/trailer
    // bytes, so Close()'s flush loop calls inner Write() at least once.
    EXPECT_THROW(gz.Close(), System::IO::IOException);
    // A second Close() (mirroring what the destructor does unconditionally) must not re-enter
    // the failing flush loop, hang, or throw again -- state_->initialized was already cleared.
    EXPECT_NO_THROW(gz.Close());
}

// Regression test for audit finding A-02 (2026-07-14): GZipStream::~GZipStream() used to call
// Close() directly with no exception handling, so letting the object go out of scope with a
// throwing inner stream called std::terminate. Reaching SUCCEED() below -- rather than the test
// process being killed -- is the actual assertion.
TEST(GZipStreamTests, Destructor_InnerWriteThrows_DoesNotTerminateProcess) {
    ThrowingWriteStream inner;
    { GZipStream gz(&inner, CompressionMode::Compress, /*leaveOpen=*/true); }
    SUCCEED();
}

TEST(GZipStreamTests, CompressThenDecompress_Roundtrip) {
    MemoryStream compressed;
    {
        GZipStream gz(&compressed, CompressionMode::Compress, /*leaveOpen=*/true);
        std::string payload = "GZipStream roundtrip payload, long enough to compress well.";
        gz.Write(reinterpret_cast<const uint8_t*>(payload.data()), 0, static_cast<int>(payload.size()));
        gz.Close();
    }

    const auto& buf = compressed.ToArray();
    // A gzip stream starts with the fixed 2-byte magic number 0x1F 0x8B.
    ASSERT_GE(buf.size(), 2u);
    EXPECT_EQ(buf[0], 0x1Fu);
    EXPECT_EQ(buf[1], 0x8Bu);

    MemoryStream source(buf.data(), static_cast<int>(buf.size()));
    GZipStream ungz(&source, CompressionMode::Decompress, /*leaveOpen=*/true);
    uint8_t out[128] = {};
    int n = ungz.Read(out, 0, 128);
    std::string result(reinterpret_cast<char*>(out), static_cast<size_t>(n));
    EXPECT_EQ(result, "GZipStream roundtrip payload, long enough to compress well.");
}

// ===========================================================================
// DeflateStream
// ===========================================================================

TEST(DeflateStreamTests, CompressMode_CanWrite_True) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_TRUE(ds.getCanWriteProperty());
}

TEST(DeflateStreamTests, CompressMode_CanRead_False) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_FALSE(ds.getCanReadProperty());
}

TEST(DeflateStreamTests, DecompressMode_CanRead_True) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    EXPECT_TRUE(ds.getCanReadProperty());
}

TEST(DeflateStreamTests, DecompressMode_CanWrite_False) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    EXPECT_FALSE(ds.getCanWriteProperty());
}

TEST(DeflateStreamTests, Read_EmptyStream_ReturnsZero) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {};
    EXPECT_EQ(ds.Read(buf, 0, 4), 0);
}

// Regression tests for audit finding A-03 (2026-07-14) -- see GZipStreamTests's identical tests
// for the full rationale.
TEST(DeflateStreamTests, Read_NullBuffer_ThrowsArgumentNullException) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    EXPECT_THROW(ds.Read(nullptr, 0, 4), System::ArgumentNullException);
}

TEST(DeflateStreamTests, Read_NegativeOffset_ThrowsArgumentOutOfRangeException) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {};
    EXPECT_THROW(ds.Read(buf, -1, 4), System::ArgumentOutOfRangeException);
}

TEST(DeflateStreamTests, Read_NegativeCount_ThrowsArgumentOutOfRangeException) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {};
    EXPECT_THROW(ds.Read(buf, 0, -1), System::ArgumentOutOfRangeException);
}

TEST(DeflateStreamTests, Write_DoesNotThrow) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    EXPECT_NO_THROW(ds.Write(buf, 0, 4));
}

// Post-stabilization audit ticket 1715: same missing-validation bug as GZipStream::Write.
TEST(DeflateStreamTests, Write_NullBuffer_ThrowsArgumentNullException) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_THROW(ds.Write(nullptr, 0, 4), System::ArgumentNullException);
}

TEST(DeflateStreamTests, Write_NegativeOffset_ThrowsArgumentOutOfRangeException) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    EXPECT_THROW(ds.Write(buf, -1, 2), System::ArgumentOutOfRangeException);
}

TEST(DeflateStreamTests, Write_NegativeCount_ThrowsArgumentOutOfRangeException) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    EXPECT_THROW(ds.Write(buf, 0, -1), System::ArgumentOutOfRangeException);
}

TEST(DeflateStreamTests, Flush_DoesNotThrow) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_NO_THROW(ds.Flush());
}

TEST(DeflateStreamTests, Close_DoesNotThrow) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_NO_THROW(ds.Close());
}

TEST(DeflateStreamTests, Close_InnerWriteThrows_PropagatesAndSecondCloseIsSafe) {
    ThrowingWriteStream inner;
    DeflateStream ds(&inner, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_THROW(ds.Close(), System::IO::IOException);
    EXPECT_NO_THROW(ds.Close());
}

// Regression test for audit finding A-02 (2026-07-14) -- see GZipStreamTests's identical test for
// the full rationale.
TEST(DeflateStreamTests, Destructor_InnerWriteThrows_DoesNotTerminateProcess) {
    ThrowingWriteStream inner;
    { DeflateStream ds(&inner, CompressionMode::Compress, /*leaveOpen=*/true); }
    SUCCEED();
}

TEST(DeflateStreamTests, CompressThenDecompress_Roundtrip) {
    MemoryStream compressed;
    {
        DeflateStream ds(&compressed, CompressionMode::Compress, /*leaveOpen=*/true);
        std::string payload = "DeflateStream roundtrip payload, long enough to compress well.";
        ds.Write(reinterpret_cast<const uint8_t*>(payload.data()), 0, static_cast<int>(payload.size()));
        ds.Close();
    }

    const auto& buf = compressed.ToArray();
    ASSERT_GT(buf.size(), 0u);

    MemoryStream source(buf.data(), static_cast<int>(buf.size()));
    DeflateStream unds(&source, CompressionMode::Decompress, /*leaveOpen=*/true);
    uint8_t out[128] = {};
    int n = unds.Read(out, 0, 128);
    std::string result(reinterpret_cast<char*>(out), static_cast<size_t>(n));
    EXPECT_EQ(result, "DeflateStream roundtrip payload, long enough to compress well.");
}

// ===========================================================================
// ZLibStream
// ===========================================================================

TEST(ZLibStreamTests, CompressMode_CanWrite_True) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_TRUE(zl.getCanWriteProperty());
}

TEST(ZLibStreamTests, CompressMode_CanRead_False) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_FALSE(zl.getCanReadProperty());
}

TEST(ZLibStreamTests, DecompressMode_CanRead_True) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    EXPECT_TRUE(zl.getCanReadProperty());
}

TEST(ZLibStreamTests, DecompressMode_CanWrite_False) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    EXPECT_FALSE(zl.getCanWriteProperty());
}

TEST(ZLibStreamTests, Read_EmptyStream_ReturnsZero) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {};
    EXPECT_EQ(zl.Read(buf, 0, 4), 0);
}

// Regression tests for audit finding A-03 (2026-07-14) -- see GZipStreamTests's identical tests
// for the full rationale.
TEST(ZLibStreamTests, Read_NullBuffer_ThrowsArgumentNullException) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    EXPECT_THROW(zl.Read(nullptr, 0, 4), System::ArgumentNullException);
}

TEST(ZLibStreamTests, Read_NegativeOffset_ThrowsArgumentOutOfRangeException) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {};
    EXPECT_THROW(zl.Read(buf, -1, 4), System::ArgumentOutOfRangeException);
}

TEST(ZLibStreamTests, Read_NegativeCount_ThrowsArgumentOutOfRangeException) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Decompress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {};
    EXPECT_THROW(zl.Read(buf, 0, -1), System::ArgumentOutOfRangeException);
}

TEST(ZLibStreamTests, Write_DoesNotThrow) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    EXPECT_NO_THROW(zl.Write(buf, 0, 4));
}

// Post-stabilization audit ticket 1715: same missing-validation bug as GZipStream::Write.
TEST(ZLibStreamTests, Write_NullBuffer_ThrowsArgumentNullException) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_THROW(zl.Write(nullptr, 0, 4), System::ArgumentNullException);
}

TEST(ZLibStreamTests, Write_NegativeOffset_ThrowsArgumentOutOfRangeException) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    EXPECT_THROW(zl.Write(buf, -1, 2), System::ArgumentOutOfRangeException);
}

TEST(ZLibStreamTests, Write_NegativeCount_ThrowsArgumentOutOfRangeException) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    EXPECT_THROW(zl.Write(buf, 0, -1), System::ArgumentOutOfRangeException);
}

TEST(ZLibStreamTests, Flush_DoesNotThrow) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_NO_THROW(zl.Flush());
}

TEST(ZLibStreamTests, Close_DoesNotThrow) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_NO_THROW(zl.Close());
}

TEST(ZLibStreamTests, Close_InnerWriteThrows_PropagatesAndSecondCloseIsSafe) {
    ThrowingWriteStream inner;
    ZLibStream zl(&inner, CompressionMode::Compress, /*leaveOpen=*/true);
    EXPECT_THROW(zl.Close(), System::IO::IOException);
    EXPECT_NO_THROW(zl.Close());
}

// Regression test for audit finding A-02 (2026-07-14) -- see GZipStreamTests's identical test for
// the full rationale.
TEST(ZLibStreamTests, Destructor_InnerWriteThrows_DoesNotTerminateProcess) {
    ThrowingWriteStream inner;
    { ZLibStream zl(&inner, CompressionMode::Compress, /*leaveOpen=*/true); }
    SUCCEED();
}

TEST(ZLibStreamTests, CompressThenDecompress_Roundtrip) {
    MemoryStream compressed;
    {
        ZLibStream zl(&compressed, CompressionMode::Compress, /*leaveOpen=*/true);
        std::string payload = "ZLibStream roundtrip payload, long enough to compress well.";
        zl.Write(reinterpret_cast<const uint8_t*>(payload.data()), 0, static_cast<int>(payload.size()));
        zl.Close();
    }

    const auto& buf = compressed.ToArray();
    // A zlib stream starts with a 2-byte header whose 16-bit value is a multiple of 31.
    ASSERT_GE(buf.size(), 2u);
    EXPECT_EQ(buf[0] & 0x0Fu, 8u);
    EXPECT_EQ((buf[0] * 256 + buf[1]) % 31, 0);

    MemoryStream source(buf.data(), static_cast<int>(buf.size()));
    ZLibStream unzl(&source, CompressionMode::Decompress, /*leaveOpen=*/true);
    uint8_t out[128] = {};
    int n = unzl.Read(out, 0, 128);
    std::string result(reinterpret_cast<char*>(out), static_cast<size_t>(n));
    EXPECT_EQ(result, "ZLibStream roundtrip payload, long enough to compress well.");
}

// ===========================================================================
// ZipArchiveMode enum
// ===========================================================================

TEST(ZipArchiveModeTests2, Read_IsZero) {
    EXPECT_EQ(static_cast<int>(ZipArchiveMode::Read), 0);
}

TEST(ZipArchiveModeTests2, Create_IsOne) {
    EXPECT_EQ(static_cast<int>(ZipArchiveMode::Create), 1);
}

TEST(ZipArchiveModeTests2, Update_IsTwo) {
    EXPECT_EQ(static_cast<int>(ZipArchiveMode::Update), 2);
}

// ===========================================================================
// Helper: build a test zip in memory using miniz directly
// ===========================================================================

static std::vector<uint8_t> makeTestZip() {
    mz_zip_archive zip{};
    mz_zip_writer_init_heap(&zip, 0, 4096);
    const char* data1 = "Hello, ZipArchive!";
    const char* data2 = "Second entry content.";
    mz_zip_writer_add_mem(&zip, "hello.txt",   data1, strlen(data1), MZ_DEFAULT_COMPRESSION);
    mz_zip_writer_add_mem(&zip, "dir/two.txt", data2, strlen(data2), MZ_DEFAULT_COMPRESSION);
    void* buf = nullptr; size_t sz = 0;
    mz_zip_writer_finalize_heap_archive(&zip, &buf, &sz);
    mz_zip_writer_end(&zip);
    std::vector<uint8_t> result(reinterpret_cast<uint8_t*>(buf),
                                 reinterpret_cast<uint8_t*>(buf) + sz);
    mz_free(buf);
    return result;
}

// ===========================================================================
// ZipArchiveEntry — default-constructed (invalid)
// ===========================================================================

TEST(ZipArchiveEntryTests, DefaultConstructed_IsInvalid) {
    ZipArchiveEntry e;
    EXPECT_FALSE(e.IsValid());
}

TEST(ZipArchiveEntryTests, DefaultConstructed_NameIsEmpty) {
    ZipArchiveEntry e;
    EXPECT_EQ(e.getNameProperty(), "");
}

TEST(ZipArchiveEntryTests, DefaultConstructed_LengthIsZero) {
    ZipArchiveEntry e;
    EXPECT_EQ(e.getLengthProperty(), 0LL);
}

// ===========================================================================
// ZipArchive — read from stream
// ===========================================================================

TEST(ZipArchiveTests, OpenFromStream_DoesNotThrow) {
    auto bytes = makeTestZip();
    MemoryStream ms(bytes.data(), static_cast<int>(bytes.size()));
    EXPECT_NO_THROW(ZipArchive z(&ms));
}

TEST(ZipArchiveTests, Entries_Count) {
    auto bytes = makeTestZip();
    MemoryStream ms(bytes.data(), static_cast<int>(bytes.size()));
    ZipArchive z(&ms);
    EXPECT_EQ(z.getEntriesProperty().size(), 2u);
}

TEST(ZipArchiveTests, Entries_Names) {
    auto bytes = makeTestZip();
    MemoryStream ms(bytes.data(), static_cast<int>(bytes.size()));
    ZipArchive z(&ms);
    auto entries = z.getEntriesProperty();
    ASSERT_EQ(entries.size(), 2u);
    // entries[0] = "hello.txt", entries[1] = "dir/two.txt"
    EXPECT_EQ(entries[0].getFullNameProperty(), "hello.txt");
    EXPECT_EQ(entries[1].getFullNameProperty(), "dir/two.txt");
}

TEST(ZipArchiveTests, Entries_BaseName) {
    auto bytes = makeTestZip();
    MemoryStream ms(bytes.data(), static_cast<int>(bytes.size()));
    ZipArchive z(&ms);
    auto entries = z.getEntriesProperty();
    EXPECT_EQ(entries[1].getNameProperty(), "two.txt");
}

TEST(ZipArchiveTests, Entries_Length) {
    auto bytes = makeTestZip();
    MemoryStream ms(bytes.data(), static_cast<int>(bytes.size()));
    ZipArchive z(&ms);
    auto entries = z.getEntriesProperty();
    EXPECT_EQ(entries[0].getLengthProperty(), (long long)strlen("Hello, ZipArchive!"));
}

TEST(ZipArchiveTests, GetEntry_Found) {
    auto bytes = makeTestZip();
    MemoryStream ms(bytes.data(), static_cast<int>(bytes.size()));
    ZipArchive z(&ms);
    auto e = z.GetEntry("hello.txt");
    EXPECT_TRUE(e.IsValid());
    EXPECT_EQ(e.getNameProperty(), "hello.txt");
}

TEST(ZipArchiveTests, GetEntry_NotFound_Invalid) {
    auto bytes = makeTestZip();
    MemoryStream ms(bytes.data(), static_cast<int>(bytes.size()));
    ZipArchive z(&ms);
    auto e = z.GetEntry("nonexistent.txt");
    EXPECT_FALSE(e.IsValid());
}

TEST(ZipArchiveTests, Open_ReadsContent) {
    auto bytes = makeTestZip();
    MemoryStream ms(bytes.data(), static_cast<int>(bytes.size()));
    ZipArchive z(&ms);
    auto e = z.GetEntry("hello.txt");
    ASSERT_TRUE(e.IsValid());
    std::unique_ptr<System::IO::Stream> s(e.Open());
    ASSERT_NE(s, nullptr);
    std::vector<uint8_t> buf(64);
    int n = s->Read(buf.data(), 0, static_cast<int>(buf.size()));
    std::string text(buf.begin(), buf.begin() + n);
    EXPECT_EQ(text, "Hello, ZipArchive!");
}

// ===========================================================================
// ZipArchive — create mode (file-based round-trip)
// ===========================================================================

TEST(ZipArchiveTests, CreateAndReadBack_RoundTrip) {
    ScopedTemporaryDirectory temporary;
    const std::string tmpPath = temporary.path("archive.zip");

    // Create
    {
        ZipArchive z(tmpPath, ZipArchiveMode::Create);
        auto entry = z.CreateEntry("greeting.txt");
        std::unique_ptr<System::IO::Stream> s(entry.Open());
        const uint8_t data[] = {'H','i','!'};
        s->Write(data, 0, 3);
        // z is disposed at end of scope → flushes zip file
    }

    // Read back
    ZipArchive z2(tmpPath, ZipArchiveMode::Read);
    auto entries = z2.getEntriesProperty();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].getFullNameProperty(), "greeting.txt");

    std::unique_ptr<System::IO::Stream> s(entries[0].Open());
    uint8_t out[8]{};
    int n = s->Read(out, 0, 8);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(out[0], 'H');
    EXPECT_EQ(out[1], 'i');
    EXPECT_EQ(out[2], '!');
}

// Regression test for ticket 309: the write-mode entry stream's Write(data, offset, count)
// used `offset` completely unchecked in `data + offset`, computing a pointer before the start
// of the caller's buffer for a negative offset -- confirmed via a standalone ASan repro as a
// genuine stack-buffer-overflow read before this fix, not just a wrong-exception-type issue.
TEST(ZipArchiveTests, EntryWriteStream_NegativeOffset_Throws) {
    ScopedTemporaryDirectory temporary;
    ZipArchive z(temporary.path("archive.zip"), ZipArchiveMode::Create);
    auto entry = z.CreateEntry("x.txt");
    std::unique_ptr<System::IO::Stream> s(entry.Open());
    const uint8_t data[] = {'a', 'b', 'c'};
    EXPECT_THROW(s->Write(data, -1, 3), System::ArgumentOutOfRangeException);
}

TEST(ZipArchiveTests, EntryWriteStream_ZeroCount_IsNoOp) {
    ScopedTemporaryDirectory temporary;
    ZipArchive z(temporary.path("archive.zip"), ZipArchiveMode::Create);
    auto entry = z.CreateEntry("x.txt");
    std::unique_ptr<System::IO::Stream> s(entry.Open());
    const uint8_t data[] = {'a', 'b', 'c'};
    EXPECT_NO_THROW(s->Write(data, 0, 0));
}

TEST(ZipArchiveTests, CreateMultipleEntries) {
    ScopedTemporaryDirectory temporary;
    const std::string tmpPath = temporary.path("archive.zip");
    {
        ZipArchive z(tmpPath, ZipArchiveMode::Create);
        for (int i = 0; i < 3; ++i) {
            auto e = z.CreateEntry("file" + std::to_string(i) + ".txt");
            std::unique_ptr<System::IO::Stream> s(e.Open());
            uint8_t byte = static_cast<uint8_t>('A' + i);
            s->Write(&byte, 0, 1);
        }
    }
    ZipArchive z2(tmpPath, ZipArchiveMode::Read);
    EXPECT_EQ(z2.getEntriesProperty().size(), 3u);
}

// Regression test for a real, silent, irreversible data-loss bug: disposing an Update-mode
// archive after CreateEntry() used to overwrite the file with ONLY the newly-created entry,
// discarding every pre-existing one. Verified against ZipArchive.cs: real .NET's Dispose()
// always preserves every entry that wasn't explicitly Delete()'d.
TEST(ZipArchiveTests, UpdateMode_PreservesExistingEntriesWhenAddingNew) {
    ScopedTemporaryDirectory temporary;
    const std::string tmpPath = temporary.path("archive.zip");

    // Seed the archive with one entry in Create mode.
    {
        ZipArchive z(tmpPath, ZipArchiveMode::Create);
        auto e = z.CreateEntry("original.txt");
        std::unique_ptr<System::IO::Stream> s(e.Open());
        const uint8_t data[] = {'O', 'l', 'd'};
        s->Write(data, 0, 3);
    }

    // Open in Update mode and add a second entry.
    {
        ZipArchive z(tmpPath, ZipArchiveMode::Update);
        auto e = z.CreateEntry("new.txt");
        std::unique_ptr<System::IO::Stream> s(e.Open());
        const uint8_t data[] = {'N', 'e', 'w'};
        s->Write(data, 0, 3);
    }

    // Both entries must be present, and the original's content must be intact.
    ZipArchive z2(tmpPath, ZipArchiveMode::Read);
    auto entries = z2.getEntriesProperty();
    ASSERT_EQ(entries.size(), 2u);

    bool foundOriginal = false, foundNew = false;
    for (auto& e : entries) {
        if (e.getFullNameProperty() == "original.txt") {
            foundOriginal = true;
            std::unique_ptr<System::IO::Stream> s(e.Open());
            uint8_t out[8]{};
            int n = s->Read(out, 0, 8);
            EXPECT_EQ(n, 3);
            EXPECT_EQ(out[0], 'O');
            EXPECT_EQ(out[1], 'l');
            EXPECT_EQ(out[2], 'd');
        } else if (e.getFullNameProperty() == "new.txt") {
            foundNew = true;
        }
    }
    EXPECT_TRUE(foundOriginal);
    EXPECT_TRUE(foundNew);
}

TEST(ZipArchiveTests, UpdateMode_DeleteExistingEntry_RemovedOnFlush) {
    ScopedTemporaryDirectory temporary;
    const std::string tmpPath = temporary.path("archive.zip");

    {
        ZipArchive z(tmpPath, ZipArchiveMode::Create);
        auto e1 = z.CreateEntry("keep.txt");
        std::unique_ptr<System::IO::Stream> s1(e1.Open());
        s1->Write(reinterpret_cast<const uint8_t*>("k"), 0, 1);
        auto e2 = z.CreateEntry("remove.txt");
        std::unique_ptr<System::IO::Stream> s2(e2.Open());
        s2->Write(reinterpret_cast<const uint8_t*>("r"), 0, 1);
    }

    {
        ZipArchive z(tmpPath, ZipArchiveMode::Update);
        auto entry = z.GetEntry("remove.txt");
        ASSERT_TRUE(entry.IsValid());
        entry.Delete();
    }

    ZipArchive z2(tmpPath, ZipArchiveMode::Read);
    auto entries = z2.getEntriesProperty();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].getFullNameProperty(), "keep.txt");
}

TEST(ZipArchiveTests, UpdateMode_DeletePendingEntry_NeverWritten) {
    ScopedTemporaryDirectory temporary;
    const std::string tmpPath = temporary.path("archive.zip");

    {
        ZipArchive z(tmpPath, ZipArchiveMode::Create);
        auto e = z.CreateEntry("existing.txt");
        std::unique_ptr<System::IO::Stream> s(e.Open());
        s->Write(reinterpret_cast<const uint8_t*>("x"), 0, 1);
    }

    {
        ZipArchive z(tmpPath, ZipArchiveMode::Update);
        auto pendingEntry = z.CreateEntry("shouldnotexist.txt");
        std::unique_ptr<System::IO::Stream> s(pendingEntry.Open());
        s->Write(reinterpret_cast<const uint8_t*>("y"), 0, 1);
        pendingEntry.Delete();
    }

    ZipArchive z2(tmpPath, ZipArchiveMode::Read);
    auto entries = z2.getEntriesProperty();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].getFullNameProperty(), "existing.txt");
}

// Regression tests for audit finding A-01 (2026-07-14): ZipArchive(Stream*, Create/Update) never
// stored the stream at all, so Dispose() finalized the archive only into an internal buffer and
// silently discarded it -- the caller-supplied stream was never written to, even though the
// public doc-comment promised "Create/Update output is written on Dispose()". Confirmed nothing
// in the existing suite caught this: OpenFromStream_DoesNotThrow only exercises Read mode, and
// CreateAndReadBack_RoundTrip uses the file-path constructor, not the stream one.

TEST(ZipArchiveTests, CreateFromStream_WritesArchiveBackToStream) {
    MemoryStream ms; // empty, growable, writable
    {
        ZipArchive z(&ms, ZipArchiveMode::Create);
        auto entry = z.CreateEntry("greeting.txt");
        std::unique_ptr<System::IO::Stream> s(entry.Open());
        const uint8_t data[] = {'H', 'i', '!'};
        s->Write(data, 0, 3);
        // z disposed at end of scope -> must write the finalized archive back into ms.
    }
    EXPECT_GT(ms.getLengthProperty(), 0) << "stream must not be empty after Create-mode Dispose()";
}

TEST(ZipArchiveTests, CreateFromStream_ReadBackThroughSameStream) {
    MemoryStream ms;
    {
        ZipArchive z(&ms, ZipArchiveMode::Create);
        auto entry = z.CreateEntry("greeting.txt");
        std::unique_ptr<System::IO::Stream> s(entry.Open());
        const uint8_t data[] = {'H', 'i', '!'};
        s->Write(data, 0, 3);
    }

    auto bytes = ms.ToArray();
    MemoryStream readBack(bytes.data(), static_cast<int>(bytes.size()));
    ZipArchive z2(&readBack, ZipArchiveMode::Read);
    auto entries = z2.getEntriesProperty();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].getFullNameProperty(), "greeting.txt");

    std::unique_ptr<System::IO::Stream> s(entries[0].Open());
    uint8_t out[8]{};
    int n = s->Read(out, 0, 8);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(out[0], 'H');
    EXPECT_EQ(out[1], 'i');
    EXPECT_EQ(out[2], '!');
}

TEST(ZipArchiveTests, UpdateFromStream_WritesChangesBackToSameStream) {
    // Seed a writable, seekable MemoryStream with one entry, then open it in Update mode, add
    // another entry, and verify the SAME stream object reflects both entries afterward --
    // exercising the seek+truncate+rewrite path. Uses the default (always-writable) MemoryStream
    // constructor + an explicit Write() to seed content, rather than the byte-array constructor
    // (MemoryStream(buffer, size) defaults to read-only in this port, unlike real .NET's
    // single-array-arg overload -- a separate, unrelated bug, not fixed here).
    MemoryStream ms;
    {
        ZipArchive z(&ms, ZipArchiveMode::Create);
        auto e = z.CreateEntry("existing.txt");
        std::unique_ptr<System::IO::Stream> s(e.Open());
        s->Write(reinterpret_cast<const uint8_t*>("x"), 0, 1);
    }
    ASSERT_GT(ms.getLengthProperty(), 0);
    ms.setPositionProperty(0); // ZipArchive's stream constructor reads from the current position

    {
        ZipArchive z(&ms, ZipArchiveMode::Update);
        auto e = z.CreateEntry("added.txt");
        std::unique_ptr<System::IO::Stream> s(e.Open());
        s->Write(reinterpret_cast<const uint8_t*>("y"), 0, 1);
        // z disposed here -- must rewrite ms in place with both entries.
    }

    auto finalBytes = ms.ToArray();
    MemoryStream readBack(finalBytes.data(), static_cast<int>(finalBytes.size()));
    ZipArchive z2(&readBack, ZipArchiveMode::Read);
    auto entries = z2.getEntriesProperty();
    ASSERT_EQ(entries.size(), 2u);
    std::vector<std::string> names;
    for (auto& e : entries) names.push_back(e.getFullNameProperty());
    std::sort(names.begin(), names.end());
    EXPECT_EQ(names, (std::vector<std::string>{"added.txt", "existing.txt"}));
}

// Regression tests for audit finding A-02 (2026-07-14): ZipArchive::~ZipArchive() used to call
// Dispose() directly with no exception handling. Dispose() can throw from the A-01 stream
// write-back code added above (stream->Write() on a failing stream) as well as from the
// pre-existing flushWriter() miniz-failure path. A throwing Dispose() escaping the destructor
// used to call std::terminate.
TEST(ZipArchiveTests, Dispose_StreamWriteThrows_Propagates) {
    ThrowingWriteStream inner;
    ZipArchive z(&inner, ZipArchiveMode::Create);
    auto e = z.CreateEntry("f.txt");
    std::unique_ptr<System::IO::Stream> s(e.Open());
    s->Write(reinterpret_cast<const uint8_t*>("x"), 0, 1);
    // Entry content is buffered internally; the throw happens on Dispose()'s write-back of the
    // finalized archive to the caller-supplied (throwing) stream.
    EXPECT_THROW(z.Dispose(), System::IO::IOException);
}

TEST(ZipArchiveTests, Destructor_StreamWriteThrows_DoesNotTerminateProcess) {
    ThrowingWriteStream inner;
    {
        ZipArchive z(&inner, ZipArchiveMode::Create);
        auto e = z.CreateEntry("f.txt");
        std::unique_ptr<System::IO::Stream> s(e.Open());
        s->Write(reinterpret_cast<const uint8_t*>("x"), 0, 1);
        // z destructed here -- Dispose() throws internally but must not escape.
    }
    SUCCEED();
}

// ===========================================================================
// ZipArchive — null Stream* (ticket #1812 / SR-AUD-242)
// ===========================================================================
//
// The Stream* constructor stored the pointer with no null check. Read and Update
// dereferenced it during construction (AddressSanitizer SEGV on 0x0); Create
// accepted it and then silently discarded the finalized archive on Dispose(),
// because the write-back is gated on a non-null stream. All three modes now throw
// ArgumentNullException("stream"), matching .NET's
// ArgumentNullException.ThrowIfNull(stream) at ZipArchive constructor entry.

TEST(ZipArchiveTests, NullStream_ReadMode_ThrowsArgumentNull) {
    EXPECT_THROW(ZipArchive z(static_cast<System::IO::Stream*>(nullptr), ZipArchiveMode::Read),
                 System::ArgumentNullException);
}

TEST(ZipArchiveTests, NullStream_UpdateMode_ThrowsArgumentNull) {
    EXPECT_THROW(ZipArchive z(static_cast<System::IO::Stream*>(nullptr), ZipArchiveMode::Update),
                 System::ArgumentNullException);
}

// Create never crashed; it lost the caller's data instead. This is the case that
// documents why the guard is unconditional rather than restricted to the crashing modes.
TEST(ZipArchiveTests, NullStream_CreateMode_ThrowsArgumentNull) {
    EXPECT_THROW(ZipArchive z(static_cast<System::IO::Stream*>(nullptr), ZipArchiveMode::Create),
                 System::ArgumentNullException);
}

// The default mode argument is Read; the defaulted overload must be rejected too.
TEST(ZipArchiveTests, NullStream_DefaultMode_ThrowsArgumentNull) {
    EXPECT_THROW(ZipArchive z(static_cast<System::IO::Stream*>(nullptr)),
                 System::ArgumentNullException);
}

TEST(ZipArchiveTests, NullStream_NamesTheStreamParameter) {
    try {
        ZipArchive z(static_cast<System::IO::Stream*>(nullptr), ZipArchiveMode::Create);
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_NE(std::string(e.what()).find("stream"), std::string::npos);
    }
}

// Rejecting construction must not leave anything half-built: no reader is opened and
// no buffer is filled, so a rejected construction is repeatable and leaks nothing.
TEST(ZipArchiveTests, NullStream_RejectedConstructionIsRepeatable) {
    for (int i = 0; i < 3; ++i) {
        EXPECT_THROW(ZipArchive z(static_cast<System::IO::Stream*>(nullptr), ZipArchiveMode::Read),
                     System::ArgumentNullException);
    }
}

// The valid paths the guard sits in front of must be byte-identical. A Create-mode
// archive over a real stream still round-trips through the same stream.
TEST(ZipArchiveTests, NullStreamGuard_DoesNotAffectValidCreateRoundTrip) {
    MemoryStream sink;
    {
        ZipArchive z(&sink, ZipArchiveMode::Create);
        auto e = z.CreateEntry("payload.txt");
        std::unique_ptr<System::IO::Stream> s(e.Open());
        s->Write(reinterpret_cast<const uint8_t*>("hello"), 0, 5);
        z.Dispose();
    }
    ASSERT_GT(sink.getLengthProperty(), 0);
    sink.setPositionProperty(0);
    ZipArchive r(&sink, ZipArchiveMode::Read);
    auto entries = r.getEntriesProperty();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].getFullNameProperty(), "payload.txt");
    EXPECT_EQ(entries[0].getLengthProperty(), 5LL);
}

// The path-based constructor is a different overload and is deliberately unchanged by
// this ticket: an unopenable path still produces InvalidDataException from the reader,
// not the new null-stream ArgumentNullException.
TEST(ZipArchiveTests, NullStreamGuard_DoesNotAffectPathConstructor) {
    ScopedTemporaryDirectory temporary;
    const std::string missing = temporary.path("missing-archive.zip");
    EXPECT_THROW(ZipArchive z(missing, ZipArchiveMode::Read), System::IO::InvalidDataException);
}

// ===========================================================================
// ZipArchive — out-of-range ZipArchiveMode (ticket #1813)
// ===========================================================================
//
// Both constructors tested the mode only with `== Read` / `== Create` / `== Update`,
// so a value outside the enumerator set took none of the branches and produced a
// zombie archive. Measured before the fix in build-probe/1813_prefix_defects.log:
//
//   cases 1-5   42, 3, -1, INT_MAX and INT_MIN all construct successfully
//   case 6      the path-taking constructor accepts 42 as well
//   case 7      ZipFile::Open inherits it, being a forwarder
//   case 9      SILENT DATA LOSS: CreateEntry + write + Dispose all accepted over a
//               real MemoryStream, and the stream receives 0 bytes
//   case 14     CreateEntry does not catch it -- that method rejects only mode == Read
//
// The defect is invisible to a sanitizer: ZipArchiveMode's implicit underlying type
// is int, so holding 42 in it is well-defined C++ and UBSan reports nothing.
//
// .NET rejects the range in ZipArchive.cs's ValidateMode default arm with
// ArgumentOutOfRangeException(nameof(mode)), reached AFTER
// ArgumentNullException.ThrowIfNull(stream) (ZipArchive.cs:135). Both facts are pinned.

TEST(ZipArchiveTests, StreamCtor_ModeAboveRange_ThrowsArgumentOutOfRange) {
    MemoryStream ms;
    EXPECT_THROW(ZipArchive z(&ms, static_cast<ZipArchiveMode>(42)),
                 System::ArgumentOutOfRangeException);
}

// One past the last enumerator: the boundary a `<= Update` style check would get wrong.
TEST(ZipArchiveTests, StreamCtor_ModeOnePastUpdate_ThrowsArgumentOutOfRange) {
    MemoryStream ms;
    EXPECT_THROW(ZipArchive z(&ms, static_cast<ZipArchiveMode>(3)),
                 System::ArgumentOutOfRangeException);
}

// One before the first enumerator.
TEST(ZipArchiveTests, StreamCtor_ModeBelowRange_ThrowsArgumentOutOfRange) {
    MemoryStream ms;
    EXPECT_THROW(ZipArchive z(&ms, static_cast<ZipArchiveMode>(-1)),
                 System::ArgumentOutOfRangeException);
}

TEST(ZipArchiveTests, StreamCtor_ModeIntMax_ThrowsArgumentOutOfRange) {
    MemoryStream ms;
    EXPECT_THROW(ZipArchive z(&ms, static_cast<ZipArchiveMode>(INT_MAX)),
                 System::ArgumentOutOfRangeException);
}

TEST(ZipArchiveTests, StreamCtor_ModeIntMin_ThrowsArgumentOutOfRange) {
    MemoryStream ms;
    EXPECT_THROW(ZipArchive z(&ms, static_cast<ZipArchiveMode>(INT_MIN)),
                 System::ArgumentOutOfRangeException);
}

TEST(ZipArchiveTests, InvalidMode_NamesTheModeParameter) {
    MemoryStream ms;
    try {
        ZipArchive z(&ms, static_cast<ZipArchiveMode>(42));
        FAIL() << "expected ArgumentOutOfRangeException";
    } catch (const System::ArgumentOutOfRangeException& e) {
        const std::string what(e.what());
        // ArgumentOutOfRangeException(nameof(mode)): .NET's default range message plus the
        // (Parameter 'mode') suffix, and nothing else.
        EXPECT_NE(what.find("Specified argument was out of the range of valid values."),
                  std::string::npos);
        EXPECT_NE(what.find("mode"), std::string::npos);
    }
}

TEST(ZipArchiveTests, PathCtor_ModeAboveRange_ThrowsArgumentOutOfRange) {
    // The path is deliberately one that does not exist: the range check must fire before
    // the file system is touched, so this is ArgumentOutOfRangeException and not the
    // InvalidDataException an unopenable path otherwise produces.
    ScopedTemporaryDirectory temporary;
    EXPECT_THROW(ZipArchive z(temporary.path("never-created.zip"),
                              static_cast<ZipArchiveMode>(42)),
                 System::ArgumentOutOfRangeException);
}

// ZipFile::Open is a bare forwarder to the path constructor, so it inherits the guard.
// Pinned separately so a future refactor that stops forwarding cannot silently lose it.
TEST(ZipArchiveTests, ZipFileOpen_ModeAboveRange_ThrowsArgumentOutOfRange) {
    ScopedTemporaryDirectory temporary;
    EXPECT_THROW(System::IO::Compression::ZipFile::Open(
                     temporary.path("never-created.zip"),
                     static_cast<ZipArchiveMode>(42)),
                 System::ArgumentOutOfRangeException);
}

// Validation order, pinned because it is observable and because it already held before the
// guard was added: the null-stream check comes first, matching ZipArchive.cs:135.
TEST(ZipArchiveTests, NullStreamBeatsInvalidMode) {
    EXPECT_THROW(ZipArchive z(static_cast<System::IO::Stream*>(nullptr),
                              static_cast<ZipArchiveMode>(42)),
                 System::ArgumentNullException);
}

// The severity of the defect: before the guard this sequence was accepted end to end and
// delivered nothing. Now it cannot start.
TEST(ZipArchiveTests, InvalidMode_CannotSilentlyDiscardAnArchive) {
    MemoryStream sink;
    EXPECT_THROW(ZipArchive z(&sink, static_cast<ZipArchiveMode>(42)),
                 System::ArgumentOutOfRangeException);
    // No partial archive state: nothing was written to the stream the rejected archive
    // would have owned.
    EXPECT_EQ(sink.getLengthProperty(), 0);
}

// A rejected construction must leave nothing half-built, and must be repeatable.
TEST(ZipArchiveTests, InvalidMode_RejectedConstructionIsRepeatable) {
    MemoryStream ms;
    for (int i = 0; i < 3; ++i) {
        EXPECT_THROW(ZipArchive z(&ms, static_cast<ZipArchiveMode>(42)),
                     System::ArgumentOutOfRangeException);
    }
    EXPECT_EQ(ms.getLengthProperty(), 0);
}

// Every valid mode keeps its exact current behaviour, through one full round trip.
TEST(ZipArchiveTests, InvalidModeGuard_LeavesAllThreeValidModesUnchanged) {
    MemoryStream sink;
    {
        ZipArchive z(&sink, ZipArchiveMode::Create);
        auto e = z.CreateEntry("payload.txt");
        std::unique_ptr<System::IO::Stream> s(e.Open());
        s->Write(reinterpret_cast<const uint8_t*>("hello"), 0, 5);
        z.Dispose();
    }
    ASSERT_GT(sink.getLengthProperty(), 0);

    sink.setPositionProperty(0);
    {
        ZipArchive u(&sink, ZipArchiveMode::Update);
        auto e = u.CreateEntry("added.txt");
        std::unique_ptr<System::IO::Stream> s(e.Open());
        s->Write(reinterpret_cast<const uint8_t*>("more"), 0, 4);
        u.Dispose();
    }

    sink.setPositionProperty(0);
    ZipArchive r(&sink, ZipArchiveMode::Read);
    EXPECT_EQ(r.getEntriesProperty().size(), 2u);
}

// The defaulted mode argument must still be accepted -- the guard must not be inverted.
TEST(ZipArchiveTests, InvalidModeGuard_DefaultedModeStillConstructs) {
    MemoryStream sink;
    {
        ZipArchive z(&sink, ZipArchiveMode::Create);
        auto e = z.CreateEntry("f.txt");
        std::unique_ptr<System::IO::Stream> s(e.Open());
        s->Write(reinterpret_cast<const uint8_t*>("x"), 0, 1);
        z.Dispose();
    }
    sink.setPositionProperty(0);
    EXPECT_NO_THROW({
        ZipArchive r(&sink);  // default mode == Read
        EXPECT_EQ(r.getEntriesProperty().size(), 1u);
    });
}

// Destructor safety: a rejected construction never runs ~ZipArchive (the throw leaves the
// constructor body), so it can neither flush nor terminate. Proven by outliving the scope.
TEST(ZipArchiveTests, InvalidMode_RejectionRunsNoDestructor) {
    MemoryStream ms;
    for (int i = 0; i < 2; ++i) {
        try {
            ZipArchive z(&ms, static_cast<ZipArchiveMode>(7));
            FAIL() << "expected ArgumentOutOfRangeException";
        } catch (const System::ArgumentOutOfRangeException&) {
            // Nothing to flush and nothing to close: no reader was opened and no stream
            // retained, so the stream is untouched and usable afterwards.
        }
    }
    EXPECT_EQ(ms.getLengthProperty(), 0);
    SUCCEED();
}

// ===========================================================================
// Ticket #1827 -- ZipArchive validates stream capabilities against the mode
// ===========================================================================
//
// The stream-capability half of .NET's ValidateModeCapabilities (ZipArchive.cs:962-975),
// unblocked by the docs/StreamCapabilityContractDesign.md §6.2 approval and added after the null
// check and #1813's mode-range check. Messages verbatim from System.IO.Compression's Strings.resx.
// Decisions this ticket made (design §6.2): Update requires read+write+seek (the harshest clause,
// since CanSeek defaults false); a Read-mode UNSEEKABLE stream is buffered, not rejected (this
// port already reads the whole input into memBuf at construction).

namespace {
    // Readable (base default true), not writable (base default false). Rejected by Create.
    class ZipReadableUnwritableStream final : public System::IO::Stream {
    public:
        System::IO::intcs Read(System::IO::bytecs*, System::IO::intcs, System::IO::intcs) override { return 0; }
        void Close() override {}
        [[nodiscard]] System::IO::intcs getLengthProperty() const override { return 0; }
    };

    // Positively unreadable. Rejected by Read and Update.
    class ZipUnreadableStream final : public System::IO::Stream {
    public:
        System::IO::intcs Read(System::IO::bytecs*, System::IO::intcs, System::IO::intcs) override { return 0; }
        void Write(const System::IO::bytecs*, System::IO::intcs, System::IO::intcs) override {}
        void Close() override {}
        [[nodiscard]] System::IO::intcs getLengthProperty() const override { return 0; }
        [[nodiscard]] bool getCanReadProperty() const override { return false; }
        [[nodiscard]] bool getCanWriteProperty() const override { return true; }
    };

    // Readable and writable but NOT seekable (CanSeek inherits false). Serves and collects bytes
    // sequentially. Accepted by Read (unseekable read is buffered), rejected by Update (no seek).
    class ZipUnseekableRWStream final : public System::IO::Stream {
        std::vector<System::IO::bytecs> buf_;
        std::size_t pos_ = 0;
    public:
        explicit ZipUnseekableRWStream(std::vector<System::IO::bytecs> b = {}) : buf_(std::move(b)) {}
        System::IO::intcs Read(System::IO::bytecs* p, System::IO::intcs off, System::IO::intcs n) override {
            System::IO::intcs k = 0;
            while (k < n && pos_ < buf_.size()) p[off + k++] = buf_[pos_++];
            return k;
        }
        void Write(const System::IO::bytecs* p, System::IO::intcs off, System::IO::intcs n) override {
            buf_.insert(buf_.end(), p + off, p + off + n);
        }
        void Close() override {}
        [[nodiscard]] System::IO::intcs getLengthProperty() const override {
            return static_cast<System::IO::intcs>(buf_.size());
        }
        [[nodiscard]] bool getCanReadProperty() const override { return true; }
        [[nodiscard]] bool getCanWriteProperty() const override { return true; }
        // getCanSeekProperty() inherits the base default of false.
    };
}

TEST(ZipArchiveTests, CreateMode_NonWritableStream_ThrowsArgumentException_1827) {
    ZipReadableUnwritableStream s;
    ASSERT_FALSE(s.getCanWriteProperty());
    try {
        ZipArchive z(&s, ZipArchiveMode::Create);
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(std::string(e.what()), "Cannot use create mode on a non-writable stream.");
    }
}

TEST(ZipArchiveTests, ReadMode_NonReadableStream_ThrowsArgumentException_1827) {
    ZipUnreadableStream s;
    ASSERT_FALSE(s.getCanReadProperty());
    try {
        ZipArchive z(&s, ZipArchiveMode::Read);
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(std::string(e.what()), "Cannot use read mode on a non-readable stream.");
    }
}

TEST(ZipArchiveTests, UpdateMode_MissingSeek_ThrowsArgumentException_1827) {
    // The harshest clause: read+write present, seek absent (the base default). .NET requires
    // all three for Update because it rewrites the central directory in place.
    ZipUnseekableRWStream s;
    ASSERT_TRUE(s.getCanReadProperty());
    ASSERT_TRUE(s.getCanWriteProperty());
    ASSERT_FALSE(s.getCanSeekProperty());
    try {
        ZipArchive z(&s, ZipArchiveMode::Update);
        FAIL() << "expected ArgumentException";
    } catch (const System::ArgumentException& e) {
        EXPECT_EQ(std::string(e.what()),
                  "Update mode requires a stream with read, write, and seek capabilities.");
    }
}

TEST(ZipArchiveTests, UpdateMode_NonReadableStream_ThrowsArgumentException_1827) {
    ZipUnreadableStream s;  // write yes, read no, seek no
    EXPECT_THROW(ZipArchive z(&s, ZipArchiveMode::Update), System::ArgumentException);
}

TEST(ZipArchiveTests, ReadMode_UnseekableReadableStream_IsBufferedNotRejected_1827) {
    // .NET buffers an unseekable Read-mode stream (isReadModeAndUnseekable); this port already
    // reads the whole input into memBuf at construction, so a readable-but-unseekable stream is
    // genuinely supported. Build a valid archive, then read it back through an unseekable stream.
    std::vector<uint8_t> bytes;
    {
        MemoryStream sink;
        {
            ZipArchive z(&sink, ZipArchiveMode::Create);
            auto e = z.CreateEntry("only.txt");
            std::unique_ptr<System::IO::Stream> s(e.Open());
            s->Write(reinterpret_cast<const uint8_t*>("hi"), 0, 2);
            z.Dispose();
        }
        bytes = sink.ToArray();
    }
    ASSERT_FALSE(bytes.empty());
    ZipUnseekableRWStream src(bytes);
    ASSERT_FALSE(src.getCanSeekProperty());
    // Must NOT throw the capability ArgumentException -- CanRead alone is required for Read mode.
    ZipArchive r(&src, ZipArchiveMode::Read);
    EXPECT_EQ(r.getEntriesProperty().size(), 1u);
}

TEST(ZipArchiveTests, Capability_NullBeatsInvalidMode_BeatsIncapableStream_1827) {
    // Validation order: null first, then #1813's mode-range, then #1827's capability. A null
    // stream with an invalid mode reports ArgumentNullException (not the mode or capability
    // error); an incapable stream with a valid mode reports the capability ArgumentException.
    EXPECT_THROW(ZipArchive z(static_cast<System::IO::Stream*>(nullptr),
                              static_cast<ZipArchiveMode>(42)),
                 System::ArgumentNullException);
    ZipReadableUnwritableStream cap;
    EXPECT_THROW(ZipArchive z(&cap, static_cast<ZipArchiveMode>(42)),
                 System::ArgumentOutOfRangeException);   // mode-range runs before capability
    EXPECT_THROW(ZipArchive z(&cap, ZipArchiveMode::Create), System::ArgumentException);
}

TEST(ZipArchiveTests, CapabilityGuard_LeavesFullyCapableStreamUnchanged_1827) {
    // The valid path for all three modes over a fully-capable MemoryStream, through a round trip.
    MemoryStream sink;
    { ZipArchive z(&sink, ZipArchiveMode::Create);
      auto e = z.CreateEntry("p.txt");
      std::unique_ptr<System::IO::Stream> s(e.Open());
      s->Write(reinterpret_cast<const uint8_t*>("data"), 0, 4);
      z.Dispose(); }
    ASSERT_GT(sink.getLengthProperty(), 0);
    sink.setPositionProperty(0);
    { ZipArchive u(&sink, ZipArchiveMode::Update);
      EXPECT_EQ(u.getEntriesProperty().size(), 1u);
      u.Dispose(); }
    sink.setPositionProperty(0);
    ZipArchive r(&sink, ZipArchiveMode::Read);
    EXPECT_EQ(r.getEntriesProperty().size(), 1u);
}

TEST(ZipArchiveTests, CapabilityRejection_RunsNoDestructor_1827) {
    // The capability guard throws before state_->mode is set and before any reader is opened or
    // buffer filled, so ~ZipArchive never runs on the rejected object -- it can neither flush nor
    // terminate. Proven by outliving the scope with the stream still usable afterwards.
    ZipReadableUnwritableStream s;
    for (int i = 0; i < 2; ++i) {
        try {
            ZipArchive z(&s, ZipArchiveMode::Create);
            FAIL() << "expected ArgumentException";
        } catch (const System::ArgumentException&) {
            // nothing retained, nothing to unwind
        }
    }
    SUCCEED();
}

// ===========================================================================
// CompressionLevel / ZipCompressionMethod / ZLibCompressionStrategy enums
// ===========================================================================

TEST(CompressionLevelTests, Values) {
    EXPECT_EQ(static_cast<int>(CompressionLevel::Optimal), 0);
    EXPECT_EQ(static_cast<int>(CompressionLevel::Fastest), 1);
    EXPECT_EQ(static_cast<int>(CompressionLevel::NoCompression), 2);
    EXPECT_EQ(static_cast<int>(CompressionLevel::SmallestSize), 3);
}

TEST(ZipCompressionMethodTests, Values) {
    EXPECT_EQ(static_cast<int>(ZipCompressionMethod::Stored), 0x0);
    EXPECT_EQ(static_cast<int>(ZipCompressionMethod::Deflate), 0x8);
    EXPECT_EQ(static_cast<int>(ZipCompressionMethod::Deflate64), 0x9);
}

TEST(ZLibCompressionStrategyTests, Values) {
    EXPECT_EQ(static_cast<int>(ZLibCompressionStrategy::Default), 0);
    EXPECT_EQ(static_cast<int>(ZLibCompressionStrategy::Filtered), 1);
    EXPECT_EQ(static_cast<int>(ZLibCompressionStrategy::HuffmanOnly), 2);
    EXPECT_EQ(static_cast<int>(ZLibCompressionStrategy::RunLengthEncoding), 3);
    EXPECT_EQ(static_cast<int>(ZLibCompressionStrategy::Fixed), 4);
}

// ===========================================================================
// ZLibCompressionOptions
// ===========================================================================

TEST(ZLibCompressionOptionsTests, Defaults) {
    ZLibCompressionOptions options;
    EXPECT_EQ(options.getCompressionLevelProperty(), -1);
    EXPECT_EQ(options.getCompressionStrategyProperty(), ZLibCompressionStrategy::Default);
    EXPECT_EQ(options.getWindowLogProperty(), -1);
}

TEST(ZLibCompressionOptionsTests, SetCompressionLevel_ValidRange) {
    ZLibCompressionOptions options;
    options.setCompressionLevelProperty(9);
    EXPECT_EQ(options.getCompressionLevelProperty(), 9);
}

TEST(ZLibCompressionOptionsTests, SetCompressionLevel_OutOfRange_Throws) {
    ZLibCompressionOptions options;
    EXPECT_THROW(options.setCompressionLevelProperty(10), System::ArgumentOutOfRangeException);
    EXPECT_THROW(options.setCompressionLevelProperty(-2), System::ArgumentOutOfRangeException);
}

TEST(ZLibCompressionOptionsTests, SetWindowLog_OutOfRange_Throws) {
    ZLibCompressionOptions options;
    EXPECT_THROW(options.setWindowLogProperty(7), System::ArgumentOutOfRangeException);
    EXPECT_THROW(options.setWindowLogProperty(16), System::ArgumentOutOfRangeException);
}

// ===========================================================================
// ZLibException
// ===========================================================================

TEST(ZLibExceptionTests, MessageCtor_WhatContainsMessage) {
    ZLibException ex("zlib failed");
    EXPECT_NE(std::string(ex.what()).find("zlib failed"), std::string::npos);
}

TEST(ZLibExceptionTests, FullCtor_StoresFields) {
    ZLibException ex("failure", "inflate", 42, "bad data");
    EXPECT_EQ(ex.getZlibErrorContextProperty(), "inflate");
    EXPECT_EQ(ex.getZlibErrorCodeProperty(), 42);
    EXPECT_EQ(ex.getZlibErrorMessageProperty(), "bad data");
}

TEST(ZLibExceptionTests, IsA_IOException) {
    EXPECT_THROW({
        try { throw ZLibException("x"); }
        catch (const System::IO::IOException&) { throw; }
    }, ZLibException);
}

// ===========================================================================
// DeflateDecoder / DeflateEncoder (streamless)
// ===========================================================================

TEST(DeflateEncoderDecoderTests, CompressThenDecompress_Roundtrip) {
    const std::string input = "The quick brown fox jumps over the lazy dog. The quick brown fox.";
    std::vector<uint8_t> compressed(256);

    DeflateEncoder encoder;
    intcs consumed = 0, written = 0;
    OperationStatus status = encoder.Compress(
        reinterpret_cast<const uint8_t*>(input.data()), static_cast<intcs>(input.size()),
        compressed.data(), static_cast<intcs>(compressed.size()), consumed, written, true);
    EXPECT_EQ(status, OperationStatus::Done);
    EXPECT_EQ(consumed, static_cast<intcs>(input.size()));
    ASSERT_GT(written, 0);

    std::vector<uint8_t> decompressed(256);
    DeflateDecoder decoder;
    intcs dConsumed = 0, dWritten = 0;
    status = decoder.Decompress(compressed.data(), written, decompressed.data(),
                                 static_cast<intcs>(decompressed.size()), dConsumed, dWritten);
    EXPECT_EQ(status, OperationStatus::Done);
    EXPECT_EQ(dWritten, static_cast<intcs>(input.size()));
    EXPECT_EQ(0, std::memcmp(decompressed.data(), input.data(), input.size()));
}

TEST(DeflateEncoderDecoderTests, TryCompress_TryDecompress_Roundtrip) {
    const std::string input = "roundtrip via static Try* helpers";
    std::vector<uint8_t> compressed(256);
    intcs written = 0;
    ASSERT_TRUE(DeflateEncoder::TryCompress(
        reinterpret_cast<const uint8_t*>(input.data()), static_cast<intcs>(input.size()),
        compressed.data(), static_cast<intcs>(compressed.size()), written));

    std::vector<uint8_t> decompressed(256);
    intcs dWritten = 0;
    ASSERT_TRUE(DeflateDecoder::TryDecompress(compressed.data(), written, decompressed.data(),
                                               static_cast<intcs>(decompressed.size()), dWritten));
    EXPECT_EQ(dWritten, static_cast<intcs>(input.size()));
    EXPECT_EQ(0, std::memcmp(decompressed.data(), input.data(), input.size()));
}

TEST(DeflateEncoderDecoderTests, Decompress_DestinationTooSmall) {
    const std::string input = "some data to compress that is reasonably long for a real test";
    std::vector<uint8_t> compressed(256);
    intcs written = 0;
    ASSERT_TRUE(DeflateEncoder::TryCompress(
        reinterpret_cast<const uint8_t*>(input.data()), static_cast<intcs>(input.size()),
        compressed.data(), static_cast<intcs>(compressed.size()), written));

    std::vector<uint8_t> tooSmall(2);
    DeflateDecoder decoder;
    intcs dConsumed = 0, dWritten = 0;
    OperationStatus status = decoder.Decompress(compressed.data(), written, tooSmall.data(),
                                                 static_cast<intcs>(tooSmall.size()), dConsumed, dWritten);
    EXPECT_EQ(status, OperationStatus::DestinationTooSmall);
}

TEST(DeflateEncoderDecoderTests, Decompress_AfterDispose_Throws) {
    DeflateDecoder decoder;
    decoder.Dispose();
    uint8_t src[1] = {0};
    uint8_t dst[1] = {0};
    intcs consumed = 0, written = 0;
    EXPECT_THROW(decoder.Decompress(src, 1, dst, 1, consumed, written), System::ObjectDisposedException);
}

TEST(DeflateEncoderDecoderTests, InvalidQuality_Throws) {
    EXPECT_THROW(DeflateEncoder(10), System::ArgumentOutOfRangeException);
    EXPECT_THROW(DeflateEncoder(-2), System::ArgumentOutOfRangeException);
}

TEST(DeflateEncoderDecoderTests, GetMaxCompressedLength_Positive) {
    EXPECT_GT(DeflateEncoder::GetMaxCompressedLength(1000), 1000);
}

TEST(DeflateEncoderDecoderTests, GetMaxCompressedLength_NegativeThrows) {
    EXPECT_THROW(DeflateEncoder::GetMaxCompressedLength(-1), System::ArgumentOutOfRangeException);
}

// Regression test: for inputs above 2^31, GetMaxCompressedLength must not truncate the length
// through a 32-bit zlib uLong before computing the bound (a real hazard on platforms where
// `unsigned long` is 32 bits, e.g. Windows) -- it must use the managed zlib-ng formula fallback
// instead and still return a value strictly larger than the (untruncated) input.
TEST(DeflateEncoderDecoderTests, GetMaxCompressedLength_AboveTwoGiB_DoesNotTruncate) {
    const SharpRuntime::longcs above2GiB = (SharpRuntime::longcs(1) << 31) + 12345;
    SharpRuntime::longcs bound = DeflateEncoder::GetMaxCompressedLength(above2GiB);
    EXPECT_GT(bound, above2GiB);
}

// ===========================================================================
// GZipDecoder / GZipEncoder (streamless)
// ===========================================================================

TEST(GZipEncoderDecoderTests, CompressThenDecompress_Roundtrip) {
    const std::string input = "gzip streamless roundtrip test data, long enough to compress well.";
    std::vector<uint8_t> compressed(256);

    GZipEncoder encoder;
    intcs consumed = 0, written = 0;
    OperationStatus status = encoder.Compress(
        reinterpret_cast<const uint8_t*>(input.data()), static_cast<intcs>(input.size()),
        compressed.data(), static_cast<intcs>(compressed.size()), consumed, written, true);
    EXPECT_EQ(status, OperationStatus::Done);

    std::vector<uint8_t> decompressed(256);
    GZipDecoder decoder;
    intcs dConsumed = 0, dWritten = 0;
    status = decoder.Decompress(compressed.data(), written, decompressed.data(),
                                 static_cast<intcs>(decompressed.size()), dConsumed, dWritten);
    EXPECT_EQ(status, OperationStatus::Done);
    EXPECT_EQ(dWritten, static_cast<intcs>(input.size()));
    EXPECT_EQ(0, std::memcmp(decompressed.data(), input.data(), input.size()));
}

TEST(GZipEncoderDecoderTests, TryCompress_TryDecompress_Roundtrip) {
    const std::string input = "another gzip roundtrip via Try* helpers";
    std::vector<uint8_t> compressed(256);
    intcs written = 0;
    ASSERT_TRUE(GZipEncoder::TryCompress(
        reinterpret_cast<const uint8_t*>(input.data()), static_cast<intcs>(input.size()),
        compressed.data(), static_cast<intcs>(compressed.size()), written));

    std::vector<uint8_t> decompressed(256);
    intcs dWritten = 0;
    ASSERT_TRUE(GZipDecoder::TryDecompress(compressed.data(), written, decompressed.data(),
                                            static_cast<intcs>(decompressed.size()), dWritten));
    EXPECT_EQ(dWritten, static_cast<intcs>(input.size()));
    EXPECT_EQ(0, std::memcmp(decompressed.data(), input.data(), input.size()));
}

TEST(GZipEncoderDecoderTests, GetMaxCompressedLength_ExceedsDeflateBoundByGzipOverhead) {
    // GZip framing (10-byte header + 8-byte CRC32/size trailer = 18 bytes) is 12 bytes more
    // than the zlib framing (2-byte header + 4-byte Adler32 trailer = 6 bytes) already
    // included in DeflateEncoder's compressBound()-derived value.
    EXPECT_EQ(GZipEncoder::GetMaxCompressedLength(1000), DeflateEncoder::GetMaxCompressedLength(1000) + 12);
}

TEST(GZipEncoderDecoderTests, GetMaxCompressedLength_NegativeThrows) {
    EXPECT_THROW(GZipEncoder::GetMaxCompressedLength(-1), System::ArgumentOutOfRangeException);
}

TEST(GZipEncoderDecoderTests, OutputIsGzipFramed) {
    // A gzip stream starts with the magic bytes 0x1F 0x8B.
    const std::string input = "check gzip framing header bytes";
    std::vector<uint8_t> compressed(256);
    intcs written = 0;
    ASSERT_TRUE(GZipEncoder::TryCompress(
        reinterpret_cast<const uint8_t*>(input.data()), static_cast<intcs>(input.size()),
        compressed.data(), static_cast<intcs>(compressed.size()), written));
    ASSERT_GE(written, 2);
    EXPECT_EQ(compressed[0], 0x1Fu);
    EXPECT_EQ(compressed[1], 0x8Bu);
}

// ===========================================================================
// ZLibDecoder / ZLibEncoder (streamless)
// ===========================================================================

TEST(ZLibEncoderDecoderTests, CompressThenDecompress_Roundtrip) {
    const std::string input = "zlib streamless roundtrip test data, long enough to compress well.";
    std::vector<uint8_t> compressed(256);

    ZLibEncoder encoder;
    intcs consumed = 0, written = 0;
    OperationStatus status = encoder.Compress(
        reinterpret_cast<const uint8_t*>(input.data()), static_cast<intcs>(input.size()),
        compressed.data(), static_cast<intcs>(compressed.size()), consumed, written, true);
    EXPECT_EQ(status, OperationStatus::Done);

    std::vector<uint8_t> decompressed(256);
    ZLibDecoder decoder;
    intcs dConsumed = 0, dWritten = 0;
    status = decoder.Decompress(compressed.data(), written, decompressed.data(),
                                 static_cast<intcs>(decompressed.size()), dConsumed, dWritten);
    EXPECT_EQ(status, OperationStatus::Done);
    EXPECT_EQ(dWritten, static_cast<intcs>(input.size()));
    EXPECT_EQ(0, std::memcmp(decompressed.data(), input.data(), input.size()));
}

TEST(ZLibEncoderDecoderTests, OutputIsZlibFramed) {
    // A zlib stream starts with a 2-byte header whose 16-bit value is a multiple of 31
    // and whose low nibble of the first byte is the compression method (8 = deflate).
    const std::string input = "check zlib framing header bytes";
    std::vector<uint8_t> compressed(256);
    intcs written = 0;
    ASSERT_TRUE(ZLibEncoder::TryCompress(
        reinterpret_cast<const uint8_t*>(input.data()), static_cast<intcs>(input.size()),
        compressed.data(), static_cast<intcs>(compressed.size()), written));
    ASSERT_GE(written, 2);
    EXPECT_EQ(compressed[0] & 0x0Fu, 8u);
    EXPECT_EQ((compressed[0] * 256 + compressed[1]) % 31, 0);
}

TEST(ZLibEncoderDecoderTests, WithCompressionOptions) {
    ZLibCompressionOptions options;
    options.setCompressionLevelProperty(9);
    options.setWindowLogProperty(9);

    const std::string input = "options-based zlib compression";
    std::vector<uint8_t> compressed(256);
    ZLibEncoder encoder(options);
    intcs consumed = 0, written = 0;
    OperationStatus status = encoder.Compress(
        reinterpret_cast<const uint8_t*>(input.data()), static_cast<intcs>(input.size()),
        compressed.data(), static_cast<intcs>(compressed.size()), consumed, written, true);
    EXPECT_EQ(status, OperationStatus::Done);

    std::vector<uint8_t> decompressed(256);
    ZLibDecoder decoder;
    intcs dConsumed = 0, dWritten = 0;
    status = decoder.Decompress(compressed.data(), written, decompressed.data(),
                                 static_cast<intcs>(decompressed.size()), dConsumed, dWritten);
    EXPECT_EQ(status, OperationStatus::Done);
    EXPECT_EQ(0, std::memcmp(decompressed.data(), input.data(), input.size()));
}

// ===========================================================================
// ZipFile / ZipFileExtensions
// ===========================================================================

TEST(ZipFileTests, CreateFromDirectory_ExtractToDirectory_Roundtrip) {
    ScopedTemporaryDirectory temporary;
    const std::string srcDir = temporary.path("source");
    const std::string archivePath = temporary.path("archive.zip");
    const std::string destDir = temporary.path("destination");

    Directory::CreateDirectory(srcDir);
    Directory::CreateDirectory(Path::Combine(srcDir, "sub"));
    File::WriteAllText(Path::Combine(srcDir, "a.txt"), "hello from a");
    File::WriteAllText(Path::Combine(srcDir, "sub", "b.txt"), "hello from b");

    ZipFile::CreateFromDirectory(srcDir, archivePath);
    ASSERT_TRUE(File::Exists(archivePath));

    ZipFile::ExtractToDirectory(archivePath, destDir);
    EXPECT_EQ(File::ReadAllText(Path::Combine(destDir, "a.txt")), "hello from a");
    EXPECT_EQ(File::ReadAllText(Path::Combine(destDir, "sub", "b.txt")), "hello from b");

}

TEST(ZipFileTests, CreateFromDirectory_IncludeBaseDirectory) {
    ScopedTemporaryDirectory temporary;
    const std::string srcDir = temporary.path("source");
    const std::string archivePath = temporary.path("archive.zip");

    Directory::CreateDirectory(srcDir);
    File::WriteAllText(Path::Combine(srcDir, "f.txt"), "data");

    ZipFile::CreateFromDirectory(srcDir, archivePath, CompressionLevel::Fastest, /*includeBaseDirectory=*/true);

    auto archive = ZipFile::OpenRead(archivePath);
    const std::string baseName = std::filesystem::path(srcDir).filename().string();
    auto entry = archive.GetEntry(baseName + "/f.txt");
    EXPECT_TRUE(entry.IsValid());

}

TEST(ZipFileTests, OpenRead_NonExistentDirectory_Throws) {
    ScopedTemporaryDirectory temporary;
    const std::string missingSource = temporary.path("missing-source");
    const std::string archivePath = temporary.path("archive.zip");
    EXPECT_THROW(
        ZipFile::CreateFromDirectory(missingSource, archivePath),
        System::IO::DirectoryNotFoundException);
}

// Regression test for a "Zip Slip" path-traversal bug: ExtractToDirectory previously combined a
// malicious entry name like "../evil.txt" with the destination directory via Path::Combine with
// no bounds check, so extraction could write files outside the specified destination directory
// entirely. Real .NET's ExtractRelativeToDirectoryCheckIfFile guards against exactly this by
// resolving the full destination path and rejecting it with an IOException unless it stays under
// the destination directory -- mirrored here.
TEST(ZipFileTests, ExtractToDirectory_EntryEscapesDestination_Throws) {
    ScopedTemporaryDirectory temporary;
    const std::string archivePath = temporary.path("archive.zip");
    const std::string destDir = temporary.path("destination");
    const std::string escapedFile = temporary.path("escaped.txt");

    {
        ZipArchive archive(archivePath, ZipArchiveMode::Create);
        auto entry = archive.CreateEntry("../escaped.txt");
        std::unique_ptr<System::IO::Stream> s(entry.Open());
        const uint8_t data[] = {'p', 'w', 'n', 'e', 'd'};
        s->Write(data, 0, 5);
    }

    EXPECT_THROW(ZipFile::ExtractToDirectory(archivePath, destDir), System::IO::IOException);
    // The escaped file must never have been written.
    EXPECT_FALSE(File::Exists(escapedFile));

}

TEST(ZipFileExtensionsTests, CreateEntryFromFile_ExtractToFile_Roundtrip) {
    ScopedTemporaryDirectory temporary;
    const std::string srcFile = temporary.path("source.txt");
    const std::string archivePath = temporary.path("archive.zip");
    const std::string destFile = temporary.path("destination.txt");

    File::WriteAllText(srcFile, "extension-method roundtrip");
    {
        ZipArchive archive(archivePath, ZipArchiveMode::Create);
        ZipFileExtensions::CreateEntryFromFile(archive, srcFile, "entry.txt");
    }

    ZipArchive archive(archivePath, ZipArchiveMode::Read);
    auto entry = archive.GetEntry("entry.txt");
    ASSERT_TRUE(entry.IsValid());
    ZipFileExtensions::ExtractToFile(entry, destFile);
    EXPECT_EQ(File::ReadAllText(destFile), "extension-method roundtrip");

}

TEST(ZipFileExtensionsTests, ExtractToFile_WithoutOverwrite_ExistingFile_Throws) {
    ScopedTemporaryDirectory temporary;
    const std::string srcFile = temporary.path("source.txt");
    const std::string archivePath = temporary.path("archive.zip");
    const std::string destFile = temporary.path("destination.txt");

    File::WriteAllText(srcFile, "content");
    File::WriteAllText(destFile, "already here");
    {
        ZipArchive archive(archivePath, ZipArchiveMode::Create);
        ZipFileExtensions::CreateEntryFromFile(archive, srcFile, "entry.txt");
    }

    ZipArchive archive(archivePath, ZipArchiveMode::Read);
    auto entry = archive.GetEntry("entry.txt");
    EXPECT_THROW(ZipFileExtensions::ExtractToFile(entry, destFile, false), System::IO::IOException);

}

TEST(ZipFileExtensionsTests, ExtractToFile_WithOverwrite_ReplacesExistingFile) {
    ScopedTemporaryDirectory temporary;
    const std::string srcFile = temporary.path("source.txt");
    const std::string archivePath = temporary.path("archive.zip");
    const std::string destFile = temporary.path("destination.txt");

    File::WriteAllText(srcFile, "new content");
    File::WriteAllText(destFile, "old content");
    {
        ZipArchive archive(archivePath, ZipArchiveMode::Create);
        ZipFileExtensions::CreateEntryFromFile(archive, srcFile, "entry.txt");
    }

    ZipArchive archive(archivePath, ZipArchiveMode::Read);
    auto entry = archive.GetEntry("entry.txt");
    EXPECT_NO_THROW(ZipFileExtensions::ExtractToFile(entry, destFile, true));
    EXPECT_EQ(File::ReadAllText(destFile), "new content");

}
