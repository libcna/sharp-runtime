// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include "System/NotImplementedException.hpp"
#include "System/ObjectDisposedException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Buffers/OperationStatus.hpp"
#include "System/IO/Compression/CompressionMode.hpp"
#include "System/IO/Compression/CompressionLevel.hpp"
#include "System/IO/Compression/GZipStream.hpp"
#include "System/IO/Compression/DeflateStream.hpp"
#include "System/IO/Compression/ZLibStream.hpp"
#include "System/IO/Compression/ZipArchive.hpp"
#include "System/IO/Compression/ZipCompressionMethod.hpp"
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
#include <cstdint>
#include <cstring>

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

TEST(GZipStreamTests, Write_DoesNotThrow) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    EXPECT_NO_THROW(gz.Write(buf, 0, 4));
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

TEST(DeflateStreamTests, Write_DoesNotThrow) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    EXPECT_NO_THROW(ds.Write(buf, 0, 4));
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

TEST(ZLibStreamTests, Write_DoesNotThrow) {
    MemoryStream ms;
    ZLibStream zl(&ms, CompressionMode::Compress, /*leaveOpen=*/true);
    SharpRuntime::bytecs buf[4] = {1, 2, 3, 4};
    EXPECT_NO_THROW(zl.Write(buf, 0, 4));
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
    const char* tmpPath = "/tmp/sharpruntimetest.zip";

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

TEST(ZipArchiveTests, CreateMultipleEntries) {
    const char* tmpPath = "/tmp/sharpruntimetest2.zip";
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
