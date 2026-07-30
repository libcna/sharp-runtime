// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#include <gtest/gtest.h>
#include "System/IO/Compression/GZipStream.hpp"
#include "System/IO/Compression/DeflateStream.hpp"
#include "System/IO/Compression/ZLibStream.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/IO/InvalidDataException.hpp"
#include "System/IO/MemoryStream.hpp"
#include "System/NotSupportedException.hpp"
#include <string>
#include <vector>
#include <cstdint>

using namespace System::IO;
using namespace System::IO::Compression;
using SharpRuntime::bytecs;
using SharpRuntime::intcs;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<bytecs> toBytes(const std::string& s) {
    return std::vector<bytecs>(s.begin(), s.end());
}

static std::vector<bytecs> gzipCompress(const std::vector<bytecs>& input) {
    MemoryStream compressed;
    {
        GZipStream gz(&compressed, CompressionMode::Compress, /*leaveOpen=*/true);
        gz.Write(input.data(), 0, static_cast<intcs>(input.size()));
        gz.Close();
    }
    return compressed.ToArray();
}

static std::vector<bytecs> gzipDecompress(const std::vector<bytecs>& compressed,
                                           intcs outputSize) {
    MemoryStream src(compressed.data(), static_cast<intcs>(compressed.size()));
    GZipStream gz(&src, CompressionMode::Decompress, /*leaveOpen=*/true);
    std::vector<bytecs> out(static_cast<size_t>(outputSize));
    intcs n = gz.Read(out.data(), 0, outputSize);
    out.resize(static_cast<size_t>(n));
    return out;
}

static std::vector<bytecs> deflateCompress(const std::vector<bytecs>& input) {
    MemoryStream compressed;
    {
        DeflateStream ds(&compressed, CompressionMode::Compress, /*leaveOpen=*/true);
        ds.Write(input.data(), 0, static_cast<intcs>(input.size()));
        ds.Close();
    }
    return compressed.ToArray();
}

static std::vector<bytecs> deflateDecompress(const std::vector<bytecs>& compressed,
                                              intcs outputSize) {
    MemoryStream src(compressed.data(), static_cast<intcs>(compressed.size()));
    DeflateStream ds(&src, CompressionMode::Decompress, /*leaveOpen=*/true);
    std::vector<bytecs> out(static_cast<size_t>(outputSize));
    intcs n = ds.Read(out.data(), 0, outputSize);
    out.resize(static_cast<size_t>(n));
    return out;
}

// ---------------------------------------------------------------------------
// GZipStream tests
// ---------------------------------------------------------------------------

TEST(GZipStream, CompressProducesGZipMagicBytes) {
    auto input = toBytes("hello");
    auto compressed = gzipCompress(input);
    ASSERT_GE(static_cast<intcs>(compressed.size()), 2);
    EXPECT_EQ(compressed[0], static_cast<bytecs>(0x1f));
    EXPECT_EQ(compressed[1], static_cast<bytecs>(0x8b));
}

TEST(GZipStream, CompressProducesNonEmptyOutput) {
    auto input = toBytes("hello world");
    auto compressed = gzipCompress(input);
    EXPECT_GT(compressed.size(), 0u);
}

TEST(GZipStream, RoundTripShortString) {
    auto input = toBytes("Hello, GZip!");
    auto compressed   = gzipCompress(input);
    auto decompressed = gzipDecompress(compressed, static_cast<intcs>(input.size()));
    EXPECT_EQ(decompressed, input);
}

TEST(GZipStream, RoundTripLongRepetitiveData) {
    // 200 KB of a repeating pattern — should compress very well
    std::vector<bytecs> input(200 * 1024);
    for (size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<bytecs>(i % 251);
    auto compressed   = gzipCompress(input);
    EXPECT_LT(compressed.size(), input.size()); // should actually compress
    auto decompressed = gzipDecompress(compressed, static_cast<intcs>(input.size()));
    EXPECT_EQ(decompressed, input);
}

TEST(GZipStream, RoundTripBinaryZeroes) {
    std::vector<bytecs> input(1024, 0);
    auto compressed   = gzipCompress(input);
    auto decompressed = gzipDecompress(compressed, static_cast<intcs>(input.size()));
    EXPECT_EQ(decompressed, input);
}

TEST(GZipStream, RoundTripSingleByte) {
    std::vector<bytecs> input = {42};
    auto compressed   = gzipCompress(input);
    auto decompressed = gzipDecompress(compressed, 1);
    EXPECT_EQ(decompressed, input);
}

TEST(GZipStream, CompressedIsSmallerThanUncompressibleForRepetitive) {
    std::string repeated(10000, 'A');
    auto input = toBytes(repeated);
    auto compressed = gzipCompress(input);
    EXPECT_LT(compressed.size(), input.size());
}

TEST(GZipStream, CanReadIsTrueForDecompress) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Decompress, true);
    EXPECT_TRUE(gz.getCanReadProperty());
    EXPECT_FALSE(gz.getCanWriteProperty());
}

TEST(GZipStream, CanWriteIsTrueForCompress) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Compress, true);
    EXPECT_FALSE(gz.getCanReadProperty());
    EXPECT_TRUE(gz.getCanWriteProperty());
}

TEST(GZipStream, GetLengthThrows) {
    MemoryStream ms;
    GZipStream gz(&ms, CompressionMode::Compress, true);
    EXPECT_THROW((void)gz.getLengthProperty(), System::NotSupportedException);
}

TEST(GZipStream, RoundTripMultipleWrites) {
    // Write input in three chunks, then decompress as one block
    auto input = toBytes("abcdefghijklmnopqrstuvwxyz0123456789");
    MemoryStream compressed;
    {
        GZipStream gz(&compressed, CompressionMode::Compress, true);
        intcs third = static_cast<intcs>(input.size()) / 3;
        gz.Write(input.data(), 0,         third);
        gz.Write(input.data(), third,     third);
        gz.Write(input.data(), third * 2, static_cast<intcs>(input.size()) - third * 2);
        gz.Close();
    }
    const auto& compBytes = compressed.ToArray();
    MemoryStream src(compBytes.data(), static_cast<intcs>(compBytes.size()));
    GZipStream gz2(&src, CompressionMode::Decompress, true);
    std::vector<bytecs> out(input.size());
    intcs n = gz2.Read(out.data(), 0, static_cast<intcs>(out.size()));
    out.resize(static_cast<size_t>(n));
    EXPECT_EQ(out, input);
}

// ---------------------------------------------------------------------------
// DeflateStream tests
// ---------------------------------------------------------------------------

TEST(DeflateStream, CompressProducesNonEmptyOutput) {
    auto input = toBytes("hello world");
    auto compressed = deflateCompress(input);
    EXPECT_GT(compressed.size(), 0u);
}

TEST(DeflateStream, RoundTripShortString) {
    auto input = toBytes("Hello, Deflate!");
    auto compressed   = deflateCompress(input);
    auto decompressed = deflateDecompress(compressed, static_cast<intcs>(input.size()));
    EXPECT_EQ(decompressed, input);
}

TEST(DeflateStream, RoundTripLongRepetitiveData) {
    std::vector<bytecs> input(200 * 1024);
    for (size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<bytecs>(i % 251);
    auto compressed   = deflateCompress(input);
    EXPECT_LT(compressed.size(), input.size());
    auto decompressed = deflateDecompress(compressed, static_cast<intcs>(input.size()));
    EXPECT_EQ(decompressed, input);
}

TEST(DeflateStream, RoundTripBinaryZeroes) {
    std::vector<bytecs> input(1024, 0);
    auto compressed   = deflateCompress(input);
    auto decompressed = deflateDecompress(compressed, static_cast<intcs>(input.size()));
    EXPECT_EQ(decompressed, input);
}

TEST(DeflateStream, RoundTripSingleByte) {
    std::vector<bytecs> input = {99};
    auto compressed   = deflateCompress(input);
    auto decompressed = deflateDecompress(compressed, 1);
    EXPECT_EQ(decompressed, input);
}

TEST(DeflateStream, NoGZipMagicHeader) {
    auto input = toBytes("test");
    auto compressed = deflateCompress(input);
    // Raw deflate must NOT start with gzip magic 0x1f 0x8b
    if (compressed.size() >= 2) {
        bool isGzip = (compressed[0] == 0x1f && compressed[1] == static_cast<bytecs>(0x8b));
        EXPECT_FALSE(isGzip);
    }
}

TEST(DeflateStream, CanReadIsTrueForDecompress) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Decompress, true);
    EXPECT_TRUE(ds.getCanReadProperty());
    EXPECT_FALSE(ds.getCanWriteProperty());
}

TEST(DeflateStream, CanWriteIsTrueForCompress) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Compress, true);
    EXPECT_FALSE(ds.getCanReadProperty());
    EXPECT_TRUE(ds.getCanWriteProperty());
}

TEST(DeflateStream, GetLengthThrows) {
    MemoryStream ms;
    DeflateStream ds(&ms, CompressionMode::Compress, true);
    EXPECT_THROW((void)ds.getLengthProperty(), System::NotSupportedException);
}

TEST(DeflateStream, DeflateAndGZipOutputDiffer) {
    auto input = toBytes("same input different format");
    auto gzipped  = gzipCompress(input);
    auto deflated = deflateCompress(input);
    // GZip has a header so bytes differ from raw deflate
    EXPECT_NE(gzipped, deflated);
}

TEST(DeflateStream, GZipCannotDecompressRawDeflate) {
    auto input    = toBytes("mismatch test");
    auto deflated = deflateCompress(input);
    MemoryStream src(deflated.data(), static_cast<intcs>(deflated.size()));
    GZipStream gz(&src, CompressionMode::Decompress, true);
    std::vector<bytecs> out(1024);
    EXPECT_THROW(gz.Read(out.data(), 0, static_cast<intcs>(out.size())), System::IO::InvalidDataException);
}

// ---------------------------------------------------------------------------
// Null inner stream — ticket #1811 / SR-AUD-257
//
// All three compression streams took a raw Stream* and none validated it, so a
// null inner stream constructed successfully and crashed at first use: a
// sufficiently large incompressible Write in Compress mode reached inner_->Write
// and a Read in Decompress mode reached inner_->Read, six ASan-confirmed SEGVs
// on address 0x0, two per type. A small compressible write does not reproduce
// it -- zlib absorbs it into the 64 KiB deflate buffer and never touches the
// inner stream -- which is why the finding specifies a large incompressible
// payload and why validating at construction, rather than at the first write, is
// what actually closes the hole.
// ---------------------------------------------------------------------------

TEST(CompressionNullStream, DeflateStreamCompressRejectsNull) {
    EXPECT_THROW(DeflateStream(nullptr, CompressionMode::Compress, true),
                 System::ArgumentNullException);
}

TEST(CompressionNullStream, DeflateStreamDecompressRejectsNull) {
    EXPECT_THROW(DeflateStream(nullptr, CompressionMode::Decompress, true),
                 System::ArgumentNullException);
}

TEST(CompressionNullStream, GZipStreamCompressRejectsNull) {
    EXPECT_THROW(GZipStream(nullptr, CompressionMode::Compress, true),
                 System::ArgumentNullException);
}

TEST(CompressionNullStream, GZipStreamDecompressRejectsNull) {
    EXPECT_THROW(GZipStream(nullptr, CompressionMode::Decompress, true),
                 System::ArgumentNullException);
}

TEST(CompressionNullStream, ZLibStreamCompressRejectsNull) {
    EXPECT_THROW(ZLibStream(nullptr, CompressionMode::Compress, true),
                 System::ArgumentNullException);
}

TEST(CompressionNullStream, ZLibStreamDecompressRejectsNull) {
    EXPECT_THROW(ZLibStream(nullptr, CompressionMode::Decompress, true),
                 System::ArgumentNullException);
}

TEST(CompressionNullStream, OwningConstructionRejectsNull) {
    // leaveOpen=false is the default and the more dangerous shape: it is the one
    // whose destructor and Close() also touch the inner stream.
    EXPECT_THROW(DeflateStream(nullptr, CompressionMode::Compress, false),
                 System::ArgumentNullException);
    EXPECT_THROW(GZipStream(nullptr, CompressionMode::Compress, false),
                 System::ArgumentNullException);
    EXPECT_THROW(ZLibStream(nullptr, CompressionMode::Compress, false),
                 System::ArgumentNullException);
}

TEST(CompressionNullStream, NullStreamNamesTheParameter) {
    try {
        DeflateStream ds(nullptr, CompressionMode::Compress, true);
        FAIL() << "expected ArgumentNullException";
    } catch (const System::ArgumentNullException& e) {
        EXPECT_NE(std::string(e.what()).find("stream"), std::string::npos)
            << "message was: " << e.what();
    }
}

TEST(CompressionNullStream, LargeIncompressibleRoundtripStillWorks) {
    // The payload shape that used to crash: large enough and random enough that
    // deflate must flush through to the inner stream repeatedly. This is the
    // regression that would fail if the check were ever placed after zlib
    // initialisation and the flush path regressed with it.
    std::vector<bytecs> input(256 * 1024);
    uint32_t x = 0x12345678u;
    for (auto& b : input) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        b = static_cast<bytecs>(x);
    }

    MemoryStream compressed;
    {
        DeflateStream ds(&compressed, CompressionMode::Compress, true);
        ds.Write(input.data(), 0, static_cast<intcs>(input.size()));
        ds.Close();
    }
    EXPECT_GT(compressed.getLengthProperty(), 0);

    auto bytes = compressed.ToArray();
    MemoryStream src(bytes.data(), static_cast<intcs>(bytes.size()));
    DeflateStream ds(&src, CompressionMode::Decompress, true);
    std::vector<bytecs> out(input.size());
    intcs total = 0;
    while (total < static_cast<intcs>(out.size())) {
        const intcs n = ds.Read(out.data(), total, static_cast<intcs>(out.size()) - total);
        if (n == 0) break;
        total += n;
    }
    EXPECT_EQ(total, static_cast<intcs>(input.size()));
    EXPECT_EQ(out, input);
}

// ---------------------------------------------------------------------------
// Closed-state capabilities (ticket #1841)
//
// All three wrappers used to return their MODE alone from getCanReadProperty() and
// getCanWriteProperty(), so a closed wrapper still claimed the capability. Real .NET's
// DeflateStream.cs:171-195 returns false whenever `_stream == null`, BEFORE consulting the
// mode, and GZipStream/ZLibStream delegate to a DeflateStream so they inherit exactly that.
//
// This is Layer 1(b) of docs/StreamCapabilityContractDesign.md: it corrects a stream lying
// about ITSELF, needs no new member and no object-layout change (`state_->initialized`
// already exists), and was measured compatible against the whole gate before it shipped.
//
// The full twelve-combination matrix is asserted -- three wrappers x two modes x
// before/after Close() -- for BOTH `leaveOpen` values, because `leaveOpen` decides whether
// `inner_` is nulled and the answer must not depend on that. Twelve, not four, because
// measuring only one wrapper would not have shown that GZipStream and ZLibStream have their
// own copies of these bodies rather than delegating as .NET's do.
//
// NOT asserted here, deliberately: .NET also conjoins the INNER stream's matching
// capability. That delegation is blocked ticket #1828 -- it consults another object's
// possibly-defaulted declaration, and System::IO::Stream::getCanWriteProperty() defaults to
// false. The current behaviour is pinned below so the split stays visible.
// ---------------------------------------------------------------------------

namespace {
    template <typename Wrapper>
    void expectCapabilitiesFalseAfterClose(CompressionMode mode, bool expectReadWhileOpen,
                                           bool leaveOpen, const char* label) {
        MemoryStream inner;
        Wrapper w(&inner, mode, leaveOpen);
        EXPECT_EQ(w.getCanReadProperty(), expectReadWhileOpen) << label << " (open, read)";
        EXPECT_EQ(w.getCanWriteProperty(), !expectReadWhileOpen) << label << " (open, write)";
        w.Close();
        EXPECT_FALSE(w.getCanReadProperty()) << label << " (closed, read)";
        EXPECT_FALSE(w.getCanWriteProperty()) << label << " (closed, write)";
    }
}

TEST(ZlibClosedCapabilityTests, DeflateStreamReportsNoCapabilityAfterClose) {
    for (bool leaveOpen : {false, true}) {
        expectCapabilitiesFalseAfterClose<DeflateStream>(
            CompressionMode::Decompress, true, leaveOpen, "DeflateStream/Decompress");
        expectCapabilitiesFalseAfterClose<DeflateStream>(
            CompressionMode::Compress, false, leaveOpen, "DeflateStream/Compress");
    }
}

TEST(ZlibClosedCapabilityTests, GZipStreamReportsNoCapabilityAfterClose) {
    for (bool leaveOpen : {false, true}) {
        expectCapabilitiesFalseAfterClose<GZipStream>(
            CompressionMode::Decompress, true, leaveOpen, "GZipStream/Decompress");
        expectCapabilitiesFalseAfterClose<GZipStream>(
            CompressionMode::Compress, false, leaveOpen, "GZipStream/Compress");
    }
}

TEST(ZlibClosedCapabilityTests, ZLibStreamReportsNoCapabilityAfterClose) {
    for (bool leaveOpen : {false, true}) {
        expectCapabilitiesFalseAfterClose<ZLibStream>(
            CompressionMode::Decompress, true, leaveOpen, "ZLibStream/Decompress");
        expectCapabilitiesFalseAfterClose<ZLibStream>(
            CompressionMode::Compress, false, leaveOpen, "ZLibStream/Compress");
    }
}

TEST(ZlibClosedCapabilityTests, ASecondCloseKeepsReportingNoCapability) {
    // The destructor calls Close() unconditionally, so the double-Close path is not
    // hypothetical; `state_->initialized` must stay false rather than being re-read from a
    // half-torn-down state.
    MemoryStream inner;
    DeflateStream ds(&inner, CompressionMode::Compress, true);
    ds.Close();
    ds.Close();
    EXPECT_FALSE(ds.getCanWriteProperty());
    EXPECT_FALSE(ds.getCanReadProperty());
}

TEST(ZlibClosedCapabilityTests, TheValidCompressPathIsUnchanged) {
    // The guard must not make an OPEN wrapper look closed: a real compress-and-close cycle
    // still reports CanWrite while open and still produces output.
    MemoryStream inner;
    const std::string text = "hello hello hello hello hello";
    const auto payload = toBytes(text);
    {
        DeflateStream ds(&inner, CompressionMode::Compress, /*leaveOpen=*/true);
        EXPECT_TRUE(ds.getCanWriteProperty());
        EXPECT_FALSE(ds.getCanReadProperty());
        ds.Write(payload.data(), 0, static_cast<intcs>(payload.size()));
        ds.Close();
        EXPECT_FALSE(ds.getCanWriteProperty());
    }
    EXPECT_GT(inner.getLengthProperty(), 0);
    EXPECT_LT(inner.getLengthProperty(), static_cast<intcs>(payload.size()));
}

TEST(ZlibClosedCapabilityTests, InnerStreamDelegationIsStillAbsent) {
    // Pins blocked ticket #1828's remaining half. .NET returns
    // `_mode == Decompress && _stream.CanRead`, so this SHOULD be false; today it is true,
    // because delegating would consult a possibly-defaulted declaration on another object.
    // When #1828 lands this assertion inverts, deliberately and visibly.
    MemoryStream inner;
    inner.Close();
    ASSERT_FALSE(inner.getCanReadProperty());
    GZipStream gz(&inner, CompressionMode::Decompress, /*leaveOpen=*/true);
    EXPECT_TRUE(gz.getCanReadProperty())
        << "if this now fails, ticket #1828's delegation half has landed -- invert it";
}
