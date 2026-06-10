// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// GZipStream and DeflateStream Read/Write throw NotImplementedException (awaiting zlib/miniz).
// ZipArchive constructor throws. CompressionMode and ZipArchiveMode enums are fully implemented.
#include <gtest/gtest.h>
#include "System/NotImplementedException.hpp"
#include "System/IO/Compression/CompressionMode.hpp"
#include "System/IO/Compression/GZipStream.hpp"
#include "System/IO/Compression/DeflateStream.hpp"
#include "System/IO/Compression/ZipArchive.hpp"
#include "System/IO/MemoryStream.hpp"

using System::NotImplementedException;
using System::IO::Compression::CompressionMode;
using System::IO::Compression::GZipStream;
using System::IO::Compression::DeflateStream;
using System::IO::Compression::ZipArchive;
using System::IO::Compression::ZipArchiveEntry;
using System::IO::Compression::ZipArchiveMode;
using System::IO::Stream;
using System::IO::MemoryStream;

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
// ZipArchive
// ===========================================================================

TEST(ZipArchiveTests, StreamConstructor_ThrowsNotImplemented) {
    MemoryStream ms;
    Stream* ptr = &ms;
    EXPECT_THROW({ ZipArchive z(ptr); }, NotImplementedException);
}

TEST(ZipArchiveTests, StringConstructor_ThrowsNotImplemented) {
    EXPECT_THROW({ ZipArchive z(std::string("archive.zip")); }, NotImplementedException);
}

TEST(ZipArchiveTests, StringConstructor_WithMode_ThrowsNotImplemented) {
    EXPECT_THROW({ ZipArchive z(std::string("archive.zip"), ZipArchiveMode::Create); }, NotImplementedException);
}

// ===========================================================================
// ZipArchiveEntry (all methods stub)
// ===========================================================================

TEST(ZipArchiveEntryTests, GetName_ThrowsNotImplemented) {
    ZipArchiveEntry e;
    EXPECT_THROW((void)e.getNameProperty(), NotImplementedException);
}

TEST(ZipArchiveEntryTests, GetFullName_ThrowsNotImplemented) {
    ZipArchiveEntry e;
    EXPECT_THROW((void)e.getFullNameProperty(), NotImplementedException);
}

TEST(ZipArchiveEntryTests, GetLength_ThrowsNotImplemented) {
    ZipArchiveEntry e;
    EXPECT_THROW((void)e.getLengthProperty(), NotImplementedException);
}

TEST(ZipArchiveEntryTests, Delete_ThrowsNotImplemented) {
    ZipArchiveEntry e;
    EXPECT_THROW(e.Delete(), NotImplementedException);
}
