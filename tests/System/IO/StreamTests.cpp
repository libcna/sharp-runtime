// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <vector>

#include "System/IO/MemoryStream.hpp"
#include "System/IO/StringReader.hpp"
#include "System/IO/StringWriter.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/NotSupportedException.hpp"

using System::IO::MemoryStream;
using System::IO::StringReader;
using System::IO::StringWriter;

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
