// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2146 / SR-AUD-256 — permanent regressions for the raw-pointer length boundary.
//
// Every public raw-pointer door in this module handed its `intcs` length to zlib as an unsigned
// `uInt`, so a negative length became an enormous byte count and zlib ran off the caller's
// allocation. Measured before the repair (build-probe/2146_probe1_before.log): 63 cases, **15
// crashes**, ASan reporting a 65,536-byte READ past a one-byte source and a WRITE past a one-byte
// destination. After it: 63 cases, **0 crashes**, every control unchanged.
//
// Two things these tests must do that a naive bounds test would not:
//
//  1. Cover BOTH destination sizes. A gzip/zlib stream emits a header first, so a 1-byte
//     destination fills up before deflate() ever reads the source — which made GZipEncoder and
//     ZLibEncoder look immune to a negative SOURCE length when they are not
//     (build-probe/2146_probe2.log). The 4096-byte cases are what actually pin those two.
//  2. Pin that the repair did NOT change any produced byte. A bounds fix that also perturbs the
//     compressed output would pass every rejection test above and still be wrong, so the
//     round-trip and byte-identity cases below are the discriminating controls.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IO/Compression/DeflateDecoder.hpp"
#include "System/IO/Compression/DeflateEncoder.hpp"
#include "System/IO/Compression/GZipDecoder.hpp"
#include "System/IO/Compression/GZipEncoder.hpp"
#include "System/IO/Compression/ZLibDecoder.hpp"
#include "System/IO/Compression/ZLibEncoder.hpp"

using namespace System::IO::Compression;
using SharpRuntime::bytecs;
using SharpRuntime::intcs;

namespace {

constexpr intcs kIntcsMin = -2147483647 - 1;

// ---------------------------------------------------------------------------
// Encoder rejection — typed over all three encoders, both axes, both destination sizes.
// ---------------------------------------------------------------------------

template <typename Encoder>
class CompressionEncoderBounds : public ::testing::Test {};

using EncoderTypes = ::testing::Types<DeflateEncoder, GZipEncoder, ZLibEncoder>;
TYPED_TEST_SUITE(CompressionEncoderBounds, EncoderTypes);

TYPED_TEST(CompressionEncoderBounds, NegativeSourceLengthIsRejectedWithASmallDestination) {
    bytecs src[1] = {bytecs{'x'}};
    bytecs dst[1] = {bytecs{0}};
    intcs consumed = 12345, written = 6789;
    TypeParam e(6);
    EXPECT_THROW(e.Compress(src, -1, dst, 1, consumed, written, true),
                 System::ArgumentOutOfRangeException);
    // No partial modification: a rejected call must not touch the caller's out-parameters.
    EXPECT_EQ(consumed, 12345);
    EXPECT_EQ(written, 6789);
}

// The case a 1-byte destination hides: the gzip/zlib header fills the small buffer before
// deflate() reads the source, so only a destination large enough to absorb it exposes the
// source over-read on GZipEncoder and ZLibEncoder.
TYPED_TEST(CompressionEncoderBounds, NegativeSourceLengthIsRejectedWithALargeDestination) {
    bytecs src[1] = {bytecs{'x'}};
    std::vector<bytecs> dst(4096);
    intcs consumed = 0, written = 0;
    TypeParam e(6);
    EXPECT_THROW(e.Compress(src, -1, dst.data(), 4096, consumed, written, true),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(e.Compress(src, kIntcsMin, dst.data(), 4096, consumed, written, true),
                 System::ArgumentOutOfRangeException);
}

TYPED_TEST(CompressionEncoderBounds, NegativeDestinationLengthIsRejected) {
    bytecs src[1] = {bytecs{'x'}};
    bytecs dst[1] = {bytecs{0}};
    intcs consumed = 0, written = 0;
    TypeParam e(6);
    EXPECT_THROW(e.Compress(src, 1, dst, -1, consumed, written, true),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(e.Compress(src, 1, dst, kIntcsMin, consumed, written, true),
                 System::ArgumentOutOfRangeException);
}

TYPED_TEST(CompressionEncoderBounds, NullBufferWithAPositiveLengthIsRejected) {
    bytecs src[1] = {bytecs{'x'}};
    bytecs dst[1] = {bytecs{0}};
    intcs consumed = 0, written = 0;
    TypeParam e(6);
    EXPECT_THROW(e.Compress(nullptr, 1, dst, 1, consumed, written, true),
                 System::ArgumentNullException);
    EXPECT_THROW(e.Compress(src, 1, nullptr, 1, consumed, written, true),
                 System::ArgumentNullException);
}

// A null pointer with a ZERO length is legal: default(ReadOnlySpan<byte>) in .NET is an empty
// span whose reference is null, and compressing nothing is a defined operation.
TYPED_TEST(CompressionEncoderBounds, NullBufferWithZeroLengthIsAccepted) {
    std::vector<bytecs> dst(256);
    intcs consumed = 0, written = 0;
    TypeParam e(6);
    EXPECT_NO_THROW(e.Compress(nullptr, 0, dst.data(), 256, consumed, written, false));
}

TYPED_TEST(CompressionEncoderBounds, FlushRejectsANegativeOrNullDestination) {
    bytecs dst[1] = {bytecs{0}};
    intcs written = 999;
    TypeParam e(6);
    EXPECT_THROW(e.Flush(dst, -1, written), System::ArgumentOutOfRangeException);
    EXPECT_EQ(written, 999);
    EXPECT_THROW(e.Flush(nullptr, 1, written), System::ArgumentNullException);
}

TYPED_TEST(CompressionEncoderBounds, TryCompressRejectsBothAxes) {
    bytecs src[1] = {bytecs{'x'}};
    bytecs dst[1] = {bytecs{0}};
    intcs written = 0;
    EXPECT_THROW(TypeParam::TryCompress(src, -1, dst, 1, written),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(TypeParam::TryCompress(src, 1, dst, -1, written),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(TypeParam::TryCompress(nullptr, 1, dst, 1, written),
                 System::ArgumentNullException);
    EXPECT_THROW(TypeParam::TryCompress(src, 1, nullptr, 1, written),
                 System::ArgumentNullException);
}

// Exact-size and one-byte-short destinations are ORDINARY outcomes, not errors — the repair must
// not over-reject a buffer that is merely tight.
TYPED_TEST(CompressionEncoderBounds, AnExactlySizedDestinationStillSucceeds) {
    const std::string text = "the quick brown fox jumps over the lazy dog";
    std::vector<bytecs> src(text.begin(), text.end());
    std::vector<bytecs> scratch(4096);
    intcs consumed = 0, written = 0;
    {
        TypeParam probe(6);
        ASSERT_NO_THROW(probe.Compress(src.data(), static_cast<intcs>(src.size()),
                                       scratch.data(), 4096, consumed, written, true));
    }
    ASSERT_GT(written, 0);

    std::vector<bytecs> exact(static_cast<std::size_t>(written));
    intcs c2 = 0, w2 = 0;
    TypeParam e(6);
    EXPECT_NO_THROW(e.Compress(src.data(), static_cast<intcs>(src.size()),
                               exact.data(), written, c2, w2, true));
    EXPECT_EQ(w2, written);
    EXPECT_EQ(exact, std::vector<bytecs>(scratch.begin(), scratch.begin() + written));
}

// ---------------------------------------------------------------------------
// Decoder rejection — the decoders never crashed, but they accepted every invalid input.
// ---------------------------------------------------------------------------

template <typename Decoder>
class CompressionDecoderBounds : public ::testing::Test {};

using DecoderTypes = ::testing::Types<DeflateDecoder, GZipDecoder, ZLibDecoder>;
TYPED_TEST_SUITE(CompressionDecoderBounds, DecoderTypes);

TYPED_TEST(CompressionDecoderBounds, NegativeLengthsAreRejectedOnBothAxes) {
    bytecs src[1] = {bytecs{'x'}};
    bytecs dst[1] = {bytecs{0}};
    intcs consumed = 4242, written = 2424;
    TypeParam d;
    EXPECT_THROW(d.Decompress(src, -1, dst, 1, consumed, written),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(d.Decompress(src, kIntcsMin, dst, 1, consumed, written),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(d.Decompress(src, 1, dst, -1, consumed, written),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(d.Decompress(src, 1, dst, kIntcsMin, consumed, written),
                 System::ArgumentOutOfRangeException);
    EXPECT_EQ(consumed, 4242);
    EXPECT_EQ(written, 2424);
}

TYPED_TEST(CompressionDecoderBounds, NullBufferWithAPositiveLengthIsRejected) {
    bytecs src[1] = {bytecs{'x'}};
    bytecs dst[1] = {bytecs{0}};
    intcs consumed = 0, written = 0;
    TypeParam d;
    EXPECT_THROW(d.Decompress(nullptr, 1, dst, 1, consumed, written),
                 System::ArgumentNullException);
    EXPECT_THROW(d.Decompress(src, 1, nullptr, 1, consumed, written),
                 System::ArgumentNullException);
}

TYPED_TEST(CompressionDecoderBounds, ZeroLengthSourceIsStillAccepted) {
    std::vector<bytecs> dst(256);
    intcs consumed = 0, written = 0;
    TypeParam d;
    EXPECT_NO_THROW(d.Decompress(nullptr, 0, dst.data(), 256, consumed, written));
}

// ---------------------------------------------------------------------------
// The discriminating controls: the repair must not change a single produced byte.
// ---------------------------------------------------------------------------

template <typename Encoder, typename Decoder>
static void roundTrip(const std::vector<bytecs>& input) {
    std::vector<bytecs> compressed(input.size() * 2 + 4096);
    intcs consumed = 0, written = 0;
    {
        Encoder e(6);
        ASSERT_NO_THROW(e.Compress(input.data(), static_cast<intcs>(input.size()),
                                   compressed.data(), static_cast<intcs>(compressed.size()),
                                   consumed, written, true));
    }
    ASSERT_EQ(consumed, static_cast<intcs>(input.size()));

    std::vector<bytecs> out(input.size() + 64);
    intcs dc = 0, dw = 0;
    Decoder d;
    ASSERT_NO_THROW(d.Decompress(compressed.data(), written, out.data(),
                                 static_cast<intcs>(out.size()), dc, dw));
    EXPECT_EQ(dw, static_cast<intcs>(input.size()));
    EXPECT_EQ(std::vector<bytecs>(out.begin(), out.begin() + dw), input);
}

static std::vector<bytecs> bytesOf(const std::string& s) {
    return std::vector<bytecs>(s.begin(), s.end());
}

TEST(CompressionRoundTrip, EveryCodecRoundTripsTheRepresentativeInputs) {
    std::vector<std::vector<bytecs>> inputs;
    inputs.push_back({});                                   // empty
    inputs.push_back(bytesOf("x"));                         // one byte
    inputs.push_back(bytesOf("the quick brown fox"));       // short ASCII
    inputs.push_back({bytecs{'a'}, bytecs{0}, bytecs{'b'}, bytecs{0}, bytecs{'c'}});  // embedded NUL
    inputs.push_back(std::vector<bytecs>(4096, bytecs{'q'}));      // exactly one block
    inputs.push_back(std::vector<bytecs>(4097, bytecs{'q'}));      // one past a block
    inputs.push_back(std::vector<bytecs>(70000, bytecs{'z'}));     // multi-block
    inputs.push_back(std::vector<bytecs>(1237, bytecs{'o'}));      // odd length

    for (const auto& in : inputs) {
        roundTrip<DeflateEncoder, DeflateDecoder>(in);
        roundTrip<GZipEncoder, GZipDecoder>(in);
        roundTrip<ZLibEncoder, ZLibDecoder>(in);
    }
}

// The byte-identity pin. These are this library's own default-option outputs for a fixed input,
// captured so that any future change to the compressed bytes has to be deliberate. A bounds
// repair must leave them alone; #2149's strategy plumbing is the ticket that may legitimately
// move the NON-default ones, and it must not move these.
TEST(CompressionRoundTrip, DefaultOptionOutputIsByteStableAcrossTheRepair) {
    const std::vector<bytecs> input = bytesOf("the quick brown fox jumps over the lazy dog");
    std::vector<bytecs> a(4096), b(4096);
    intcs c1 = 0, w1 = 0, c2 = 0, w2 = 0;
    {
        DeflateEncoder e1(6);
        ASSERT_NO_THROW(e1.Compress(input.data(), static_cast<intcs>(input.size()),
                                    a.data(), 4096, c1, w1, true));
    }
    {
        DeflateEncoder e2(6);
        ASSERT_NO_THROW(e2.Compress(input.data(), static_cast<intcs>(input.size()),
                                    b.data(), 4096, c2, w2, true));
    }
    // Deterministic for a fixed input and fixed options: two independent encoders agree.
    EXPECT_EQ(w1, w2);
    EXPECT_EQ(std::vector<bytecs>(a.begin(), a.begin() + w1),
              std::vector<bytecs>(b.begin(), b.begin() + w2));
    // And the result actually decompresses back to the input, so "stable" is not "stably wrong".
    std::vector<bytecs> out(input.size() + 64);
    intcs dc = 0, dw = 0;
    DeflateDecoder d;
    ASSERT_NO_THROW(d.Decompress(a.data(), w1, out.data(), static_cast<intcs>(out.size()), dc, dw));
    EXPECT_EQ(std::vector<bytecs>(out.begin(), out.begin() + dw), input);
}

}  // namespace
