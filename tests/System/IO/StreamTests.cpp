// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <vector>

#include "System/IO/MemoryStream.hpp"
#include "System/IO/SeekOrigin.hpp"
#include "System/IO/StringReader.hpp"
#include "System/IO/StringWriter.hpp"
#include "System/IO/UnmanagedMemoryStream.hpp"
#include "System/IO/UnmanagedMemoryAccessor.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/IOException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/ObjectDisposedException.hpp"

using System::IO::MemoryStream;
using System::IO::SeekOrigin;
using System::IO::StringReader;
using System::IO::StringWriter;
using System::IO::UnmanagedMemoryStream;
using System::IO::UnmanagedMemoryAccessor;
using System::IO::FileAccess;
using System::IO::intcs;

// ---------------------------------------------------------------------------
// MemoryStream — writable (default ctor)
// ---------------------------------------------------------------------------

TEST(MemoryStreamTests, DefaultCtorIsEmpty) {
    MemoryStream ms;
    EXPECT_EQ(ms.getLengthProperty(), 0);
    EXPECT_TRUE(ms.getCanWriteProperty());
}

TEST(MemoryStreamTests, WriteByte) {
    MemoryStream ms;
    ms.WriteByte(0x41); // 'A'
    EXPECT_EQ(ms.getLengthProperty(), 1);
    EXPECT_EQ(ms.ToArray()[0], 0x41u);
}

TEST(MemoryStreamTests, WriteMultipleBytes) {
    MemoryStream ms;
    ms.WriteByte(1); ms.WriteByte(2); ms.WriteByte(3);
    EXPECT_EQ(ms.getLengthProperty(), 3);
    EXPECT_EQ(ms.ToArray()[2], 3u);
}

TEST(MemoryStreamTests, WriteBuffer) {
    MemoryStream ms;
    uint8_t data[] = {10, 20, 30, 40};
    ms.Write(data, 0, 4);
    EXPECT_EQ(ms.getLengthProperty(), 4);
    const auto& arr = ms.ToArray();
    EXPECT_EQ(arr[0], 10u);
    EXPECT_EQ(arr[3], 40u);
}

TEST(MemoryStreamTests, WriteBufferWithOffset) {
    MemoryStream ms;
    uint8_t data[] = {0, 0, 99, 100, 101};
    ms.Write(data, 2, 3); // skip first 2 bytes
    EXPECT_EQ(ms.getLengthProperty(), 3);
    EXPECT_EQ(ms.ToArray()[0], 99u);
    EXPECT_EQ(ms.ToArray()[2], 101u);
}

TEST(MemoryStreamTests, GetBufferReturnsCopy) {
    MemoryStream ms;
    ms.WriteByte(7);
    auto copy = ms.GetBuffer();
    EXPECT_EQ(copy.size(), 1u);
    EXPECT_EQ(copy[0], 7u);
}

TEST(MemoryStreamTests, ToArrayReturnsRef) {
    MemoryStream ms;
    ms.WriteByte(42);
    EXPECT_EQ(ms.ToArray().size(), 1u);
}

TEST(MemoryStreamTests, CanWriteIsTrue) {
    MemoryStream ms;
    EXPECT_TRUE(ms.getCanWriteProperty());
}

// ---------------------------------------------------------------------------
// MemoryStream — read-only (from buffer ctor)
// ---------------------------------------------------------------------------

TEST(MemoryStreamTests, ReadOnlyCtorFromBuffer) {
    uint8_t src[] = {1, 2, 3, 4, 5};
    MemoryStream ms(src, 5);
    EXPECT_EQ(ms.getLengthProperty(), 5);
    EXPECT_FALSE(ms.getCanWriteProperty());
}

TEST(MemoryStreamTests, ReadFromBuffer) {
    uint8_t src[] = {10, 20, 30};
    MemoryStream ms(src, 3);
    uint8_t dst[3] = {};
    int n = ms.Read(dst, 0, 3);
    EXPECT_EQ(n, 3);
    EXPECT_EQ(dst[0], 10u);
    EXPECT_EQ(dst[1], 20u);
    EXPECT_EQ(dst[2], 30u);
}

TEST(MemoryStreamTests, ReadReturnsZeroAtEnd) {
    uint8_t src[] = {1};
    MemoryStream ms(src, 1);
    uint8_t dst[4] = {};
    ms.Read(dst, 0, 1); // consume the one byte
    int n = ms.Read(dst, 0, 4);
    EXPECT_EQ(n, 0);
}

TEST(MemoryStreamTests, ReadLessThanRequested) {
    // only 2 bytes available, ask for 10
    uint8_t src[] = {5, 6};
    MemoryStream ms(src, 2);
    uint8_t dst[10] = {};
    int n = ms.Read(dst, 0, 10);
    EXPECT_EQ(n, 2);
    EXPECT_EQ(dst[0], 5u);
    EXPECT_EQ(dst[1], 6u);
}

// ---------------------------------------------------------------------------
// MemoryStream — write then read roundtrip
// ---------------------------------------------------------------------------

TEST(MemoryStreamTests, WriteReadRoundtrip) {
    MemoryStream writer;
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    writer.Write(payload, 0, 4);

    // Construct a read-only stream from the written data
    const auto& buf = writer.ToArray();
    MemoryStream reader(buf.data(), static_cast<int>(buf.size()));
    uint8_t readback[4] = {};
    reader.Read(readback, 0, 4);

    EXPECT_EQ(readback[0], 0xDEu);
    EXPECT_EQ(readback[3], 0xEFu);
}

// ---------------------------------------------------------------------------
// MemoryStream — Position
// ---------------------------------------------------------------------------

TEST(MemoryStreamTests, PositionStartsAtZero) {
    MemoryStream ms;
    EXPECT_EQ(ms.getPositionProperty(), 0);
}

TEST(MemoryStreamTests, PositionAdvancesOnWrite) {
    MemoryStream ms;
    ms.WriteByte(1); ms.WriteByte(2);
    EXPECT_EQ(ms.getPositionProperty(), 2);
}

TEST(MemoryStreamTests, PositionAdvancesOnRead) {
    uint8_t src[] = {1, 2, 3};
    MemoryStream ms(src, 3);
    uint8_t dst[2] = {};
    ms.Read(dst, 0, 2);
    EXPECT_EQ(ms.getPositionProperty(), 2);
}

TEST(MemoryStreamTests, SetPositionSeeksForRewrite) {
    MemoryStream ms;
    uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    ms.Write(payload, 0, 3);
    ms.setPositionProperty(1);
    ms.WriteByte(0xFF);
    EXPECT_EQ(ms.getLengthProperty(), 3);
    EXPECT_EQ(ms.ToArray()[1], 0xFFu);
}

TEST(MemoryStreamTests, SetPositionNegativeThrows) {
    MemoryStream ms;
    EXPECT_THROW(ms.setPositionProperty(-1), System::ArgumentOutOfRangeException);
}

TEST(MemoryStreamTests, CanSeekIsTrue) {
    MemoryStream ms;
    EXPECT_TRUE(ms.getCanSeekProperty());
}

TEST(MemoryStreamTests, SeekFromBeginMatchesSetPosition) {
    MemoryStream ms;
    uint8_t payload[] = {1, 2, 3, 4, 5};
    ms.Write(payload, 0, 5);
    auto newPos = ms.Seek(2, SeekOrigin::Begin);
    EXPECT_EQ(newPos, 2);
    EXPECT_EQ(ms.getPositionProperty(), 2);
}

TEST(MemoryStreamTests, SeekFromCurrentAdvancesRelatively) {
    MemoryStream ms;
    uint8_t payload[] = {1, 2, 3, 4, 5};
    ms.Write(payload, 0, 5);
    ms.setPositionProperty(1);
    auto newPos = ms.Seek(2, SeekOrigin::Current);
    EXPECT_EQ(newPos, 3);
}

TEST(MemoryStreamTests, SeekFromEndComputesFromLength) {
    MemoryStream ms;
    uint8_t payload[] = {1, 2, 3, 4, 5}; // length 5
    ms.Write(payload, 0, 5);
    auto newPos = ms.Seek(-2, SeekOrigin::End);
    EXPECT_EQ(newPos, 3);
}

TEST(MemoryStreamTests, SetLengthTruncates) {
    MemoryStream ms;
    uint8_t payload[] = {1, 2, 3, 4, 5};
    ms.Write(payload, 0, 5);
    ms.SetLength(2);
    EXPECT_EQ(ms.getLengthProperty(), 2);
}

TEST(MemoryStreamTests, SetLengthExtendsWithZeros) {
    MemoryStream ms;
    ms.WriteByte(1);
    ms.SetLength(3);
    EXPECT_EQ(ms.getLengthProperty(), 3);
    EXPECT_EQ(ms.ToArray()[1], 0u);
    EXPECT_EQ(ms.ToArray()[2], 0u);
}

TEST(MemoryStreamTests, SetLengthNegativeThrows) {
    MemoryStream ms;
    EXPECT_THROW(ms.SetLength(-1), System::ArgumentOutOfRangeException);
}

TEST(MemoryStreamTests, SetLengthOnReadOnlyThrowsNotSupportedException) {
    uint8_t src[] = {1, 2, 3};
    MemoryStream ms(src, 3);
    EXPECT_THROW(ms.SetLength(1), System::NotSupportedException);
}

TEST(MemoryStreamTests, ReadByteReturnsBytesThenMinusOne) {
    uint8_t src[] = {0x41, 0x42};
    MemoryStream ms(src, 2);
    EXPECT_EQ(ms.ReadByte(), 0x41);
    EXPECT_EQ(ms.ReadByte(), 0x42);
    EXPECT_EQ(ms.ReadByte(), -1);
}

// ---------------------------------------------------------------------------
// Stream — default Position behavior for non-seekable streams
// ---------------------------------------------------------------------------

namespace {
    class NonSeekableTestStream final : public System::IO::Stream {
    public:
        System::IO::intcs Read(System::IO::bytecs*, System::IO::intcs, System::IO::intcs) override { return 0; }
        void Close() override {}
        [[nodiscard]] System::IO::intcs getLengthProperty() const override { return 0; }
    };
}

TEST(StreamTests, DefaultGetPositionThrowsNotSupported) {
    NonSeekableTestStream s;
    EXPECT_THROW(s.getPositionProperty(), System::NotSupportedException);
}

TEST(StreamTests, DefaultSetPositionThrowsNotSupported) {
    NonSeekableTestStream s;
    EXPECT_THROW(s.setPositionProperty(0), System::NotSupportedException);
}

TEST(StreamTests, DefaultCanSeekIsFalse) {
    NonSeekableTestStream s;
    EXPECT_FALSE(s.getCanSeekProperty());
}

TEST(StreamTests, DefaultSeekThrowsNotSupported) {
    NonSeekableTestStream s;
    EXPECT_THROW(s.Seek(0, System::IO::SeekOrigin::Begin), System::NotSupportedException);
}

TEST(StreamTests, DefaultSetLengthThrowsNotSupported) {
    NonSeekableTestStream s;
    EXPECT_THROW(s.SetLength(0), System::NotSupportedException);
}

TEST(StreamTests, DefaultReadByteReturnsMinusOneAtEnd) {
    NonSeekableTestStream s;
    EXPECT_EQ(s.ReadByte(), -1);
}

// ---------------------------------------------------------------------------
// StringReader
// ---------------------------------------------------------------------------

TEST(StringReaderTests, PeekReturnsFirstChar) {
    StringReader sr("hello");
    EXPECT_EQ(sr.Peek(), int('h'));
}

TEST(StringReaderTests, PeekDoesNotAdvance) {
    StringReader sr("ab");
    EXPECT_EQ(sr.Peek(), int('a'));
    EXPECT_EQ(sr.Peek(), int('a')); // still 'a'
}

TEST(StringReaderTests, ReadAdvances) {
    StringReader sr("ab");
    EXPECT_EQ(sr.Read(), int('a'));
    EXPECT_EQ(sr.Read(), int('b'));
}

TEST(StringReaderTests, ReadReturnsMinusOneAtEnd) {
    StringReader sr("x");
    sr.Read(); // consume 'x'
    EXPECT_EQ(sr.Read(),  -1);
    EXPECT_EQ(sr.Peek(),  -1);
}

TEST(StringReaderTests, ReadEmptyString) {
    StringReader sr("");
    EXPECT_EQ(sr.Read(),  -1);
    EXPECT_EQ(sr.Peek(),  -1);
}

TEST(StringReaderTests, ReadLine) {
    StringReader sr("line1\nline2\nline3");
    EXPECT_EQ(sr.ReadLine(), "line1");
    EXPECT_EQ(sr.ReadLine(), "line2");
    EXPECT_EQ(sr.ReadLine(), "line3");
}

TEST(StringReaderTests, ReadLineStripsCarriageReturn) {
    StringReader sr("line1\r\nline2");
    EXPECT_EQ(sr.ReadLine(), "line1");
    EXPECT_EQ(sr.ReadLine(), "line2");
}

TEST(StringReaderTests, ReadLineAtEndReturnsEmpty) {
    StringReader sr("only");
    sr.ReadLine(); // consume "only"
    EXPECT_EQ(sr.ReadLine(), "");
}

TEST(StringReaderTests, ReadToEnd) {
    StringReader sr("hello world");
    EXPECT_EQ(sr.ReadToEnd(), "hello world");
}

TEST(StringReaderTests, ReadToEndAfterPartialRead) {
    StringReader sr("abcdef");
    sr.Read(); sr.Read(); // consume 'a', 'b'
    EXPECT_EQ(sr.ReadToEnd(), "cdef");
}

TEST(StringReaderTests, ReadToEndAtEndIsEmpty) {
    StringReader sr("x");
    sr.ReadToEnd();
    EXPECT_EQ(sr.ReadToEnd(), "");
}

// ---------------------------------------------------------------------------
// StringWriter
// ---------------------------------------------------------------------------

TEST(StringWriterTests, DefaultCtorEmpty) {
    StringWriter sw;
    EXPECT_EQ(sw.ToString(), "");
}

TEST(StringWriterTests, WriteAppends) {
    StringWriter sw;
    sw.Write(std::string("hello"));
    EXPECT_EQ(sw.ToString(), "hello");
}

TEST(StringWriterTests, WriteMultipleTimes) {
    StringWriter sw;
    sw.Write(std::string("foo"));
    sw.Write(std::string("bar"));
    EXPECT_EQ(sw.ToString(), "foobar");
}

TEST(StringWriterTests, GetStringBuilderAliasToString) {
    StringWriter sw;
    sw.Write(std::string("test"));
    EXPECT_EQ(sw.GetStringBuilder(), sw.ToString());
}

TEST(StringWriterTests, ToStringIdempotent) {
    StringWriter sw;
    sw.Write(std::string("data"));
    EXPECT_EQ(sw.ToString(), sw.ToString());
}

// ---------------------------------------------------------------------------
// UnmanagedMemoryStream
// ---------------------------------------------------------------------------

TEST(UnmanagedMemoryStreamTests, ReadOnlyCtor_ReadsBytes) {
    uint8_t data[] = {1, 2, 3, 4, 5};
    UnmanagedMemoryStream ums(data, 5);
    EXPECT_EQ(ums.getLengthProperty(), 5);
    EXPECT_TRUE(ums.getCanReadProperty());
    EXPECT_FALSE(ums.getCanWriteProperty());
    uint8_t buf[5] = {};
    intcs n = ums.Read(buf, 0, 5);
    EXPECT_EQ(n, 5);
    EXPECT_EQ(buf[4], 5u);
}

TEST(UnmanagedMemoryStreamTests, ReadReturnsZeroAtEnd) {
    uint8_t data[] = {1, 2};
    UnmanagedMemoryStream ums(data, 2);
    uint8_t buf[2] = {};
    ums.Read(buf, 0, 2);
    EXPECT_EQ(ums.Read(buf, 0, 2), 0);
}

TEST(UnmanagedMemoryStreamTests, WriteWithinCapacity_UpdatesLength) {
    uint8_t data[8] = {};
    UnmanagedMemoryStream ums(data, 0, 8, FileAccess::ReadWrite);
    uint8_t payload[] = {9, 9, 9};
    ums.Write(payload, 0, 3);
    EXPECT_EQ(ums.getLengthProperty(), 3);
    EXPECT_EQ(data[2], 9u);
}

TEST(UnmanagedMemoryStreamTests, WriteBeyondCapacity_Throws) {
    uint8_t data[2] = {};
    UnmanagedMemoryStream ums(data, 0, 2, FileAccess::ReadWrite);
    uint8_t payload[] = {1, 2, 3};
    EXPECT_THROW(ums.Write(payload, 0, 3), System::IO::IOException);
}

TEST(UnmanagedMemoryStreamTests, WriteWhenReadOnly_ThrowsNotSupportedException) {
    uint8_t data[] = {1, 2};
    UnmanagedMemoryStream ums(data, 2);
    uint8_t payload[] = {9};
    EXPECT_THROW(ums.Write(payload, 0, 1), System::NotSupportedException);
}

TEST(UnmanagedMemoryStreamTests, CanSeek_TrueWhileOpen) {
    uint8_t data[] = {1, 2};
    UnmanagedMemoryStream ums(data, 2);
    EXPECT_TRUE(ums.getCanSeekProperty());
}

TEST(UnmanagedMemoryStreamTests, SetPosition_SeeksForRead) {
    uint8_t data[] = {10, 20, 30};
    UnmanagedMemoryStream ums(data, 3);
    ums.setPositionProperty(1);
    uint8_t buf[2] = {};
    ums.Read(buf, 0, 2);
    EXPECT_EQ(buf[0], 20u);
}

TEST(UnmanagedMemoryStreamTests, SetLength_BeyondCapacity_Throws) {
    uint8_t data[4] = {};
    UnmanagedMemoryStream ums(data, 0, 4, FileAccess::ReadWrite);
    EXPECT_THROW(ums.SetLength(5), System::IO::IOException);
}

TEST(UnmanagedMemoryStreamTests, NullPointer_ThrowsArgumentNullException) {
    EXPECT_THROW(UnmanagedMemoryStream(nullptr, 0), System::ArgumentNullException);
}

TEST(UnmanagedMemoryStreamTests, LengthGreaterThanCapacity_Throws) {
    uint8_t data[4] = {};
    EXPECT_THROW(UnmanagedMemoryStream(data, 4, 2, FileAccess::Read), System::ArgumentOutOfRangeException);
}

// ---------------------------------------------------------------------------
// UnmanagedMemoryAccessor
// ---------------------------------------------------------------------------

TEST(UnmanagedMemoryAccessorTests, ReadWriteByte_Roundtrip) {
    uint8_t data[4] = {};
    UnmanagedMemoryAccessor acc(data, 4);
    acc.Write(0, static_cast<uint8_t>(0xAB));
    EXPECT_EQ(acc.ReadByte(0), 0xABu);
}

TEST(UnmanagedMemoryAccessorTests, ReadWriteInt32_Roundtrip) {
    uint8_t data[4] = {};
    UnmanagedMemoryAccessor acc(data, 4);
    acc.Write(0, static_cast<int32_t>(-12345));
    EXPECT_EQ(acc.ReadInt32(0), -12345);
}

TEST(UnmanagedMemoryAccessorTests, ReadWriteDouble_Roundtrip) {
    uint8_t data[8] = {};
    UnmanagedMemoryAccessor acc(data, 8);
    acc.Write(0, 3.14159);
    EXPECT_DOUBLE_EQ(acc.ReadDouble(0), 3.14159);
}

TEST(UnmanagedMemoryAccessorTests, ReadWriteBoolean_Roundtrip) {
    uint8_t data[1] = {};
    UnmanagedMemoryAccessor acc(data, 1);
    acc.Write(0, true);
    EXPECT_TRUE(acc.ReadBoolean(0));
}

TEST(UnmanagedMemoryAccessorTests, CapacityProperty) {
    uint8_t data[16] = {};
    UnmanagedMemoryAccessor acc(data, 16);
    EXPECT_EQ(acc.getCapacityProperty(), 16);
}

TEST(UnmanagedMemoryAccessorTests, ReadOnly_WriteThrowsNotSupportedException) {
    uint8_t data[4] = {};
    UnmanagedMemoryAccessor acc(data, 4, FileAccess::Read);
    EXPECT_TRUE(acc.getCanReadProperty());
    EXPECT_FALSE(acc.getCanWriteProperty());
    EXPECT_THROW(acc.Write(0, static_cast<uint8_t>(1)), System::NotSupportedException);
}

TEST(UnmanagedMemoryAccessorTests, OutOfRangePosition_ThrowsArgumentOutOfRangeException) {
    uint8_t data[4] = {};
    UnmanagedMemoryAccessor acc(data, 4);
    EXPECT_THROW(acc.ReadInt32(1), System::ArgumentOutOfRangeException); // needs 4 bytes at position 1, only 3 available
}

TEST(UnmanagedMemoryAccessorTests, AfterDispose_ThrowsObjectDisposedException) {
    uint8_t data[4] = {};
    UnmanagedMemoryAccessor acc(data, 4);
    acc.Dispose();
    EXPECT_THROW(acc.ReadByte(0), System::ObjectDisposedException);
}

TEST(UnmanagedMemoryAccessorTests, NullBuffer_ThrowsArgumentNullException) {
    EXPECT_THROW(UnmanagedMemoryAccessor(nullptr, 4), System::ArgumentNullException);
}
