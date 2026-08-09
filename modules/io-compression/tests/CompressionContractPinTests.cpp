// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2151 — the closing ticket of the System::IO::Compression review (#2147).
//
// Zero executable production change. This file pins the two things the review deliberately did NOT
// change, so neither can land silently:
//
//   1. the ABSENCE of the three stream types' ZLibCompressionOptions constructors -- the remaining
//      half of SR-AUD-259, which is a public surface addition and belongs to the BLOCKED #2150;
//   2. the raw-pointer contract #2146 established and #2151 wrote into the six codec headers,
//      asserted at the doors the header text names, so the documentation cannot drift from the code.
//
// NOTE on the ticket's original acceptance criterion: it also asked for pins on "the current
// CompressionMode acceptance, the current Write-after-Close silence, and the current
// strategy-is-ignored behaviour". Those three behaviours were REPAIRED in the same batch, by #2148
// and #2149, so pinning them as current would pin the defects. Their replacements are pinned by
// CompressionStreamStateTests and CompressionStrategyTests instead. The criterion was written
// assuming #2148/#2149 would not land first; the correction is recorded in the plan's §16 and in
// the ticket's own notes rather than applied silently.

#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IO/Compression/DeflateDecoder.hpp"
#include "System/IO/Compression/DeflateEncoder.hpp"
#include "System/IO/Compression/DeflateStream.hpp"
#include "System/IO/Compression/GZipDecoder.hpp"
#include "System/IO/Compression/GZipEncoder.hpp"
#include "System/IO/Compression/GZipStream.hpp"
#include "System/IO/Compression/ZLibCompressionOptions.hpp"
#include "System/IO/Compression/ZLibDecoder.hpp"
#include "System/IO/Compression/ZLibEncoder.hpp"
#include "System/IO/Compression/ZLibStream.hpp"
#include "System/IO/MemoryStream.hpp"

using namespace System::IO;
using namespace System::IO::Compression;
using SharpRuntime::bytecs;
using SharpRuntime::intcs;

// ---------------------------------------------------------------------------
// 1. #2150's public surface is ABSENT — pinned at compile time
// ---------------------------------------------------------------------------

namespace {

    template <typename Stream>
    constexpr bool hasOptionsConstructor =
        std::is_constructible_v<Stream, System::IO::Stream*, const ZLibCompressionOptions&> ||
        std::is_constructible_v<Stream, System::IO::Stream*, const ZLibCompressionOptions&, bool>;

    // The pin. Current .NET gives DeflateStream, GZipStream and ZLibStream a constructor taking
    // ZLibCompressionOptions; this module has none, and adding them is a public surface addition
    // that ticket #2150 holds BLOCKED pending approval. If #2150 ever lands, this assertion fails
    // and forces the change to be deliberate.
    static_assert(!hasOptionsConstructor<DeflateStream>, "#2150 landed without updating its pin");
    static_assert(!hasOptionsConstructor<GZipStream>, "#2150 landed without updating its pin");
    static_assert(!hasOptionsConstructor<ZLibStream>, "#2150 landed without updating its pin");

    // The companion that stops the three assertions above from being vacuously true: the trait
    // must report `true` for the constructor the module DOES have.
    static_assert(std::is_constructible_v<DeflateStream, System::IO::Stream*, CompressionMode, bool>);
    static_assert(std::is_constructible_v<GZipStream, System::IO::Stream*, CompressionMode, bool>);
    static_assert(std::is_constructible_v<ZLibStream, System::IO::Stream*, CompressionMode, bool>);

} // namespace

TEST(CompressionContractPinTests, TheStreamTypesStillHaveNoOptionsConstructor_SeeBlockedTicket2150) {
    // A runtime home for the compile-time pins above, so the omission is visible in the test
    // listing and not only in a header nobody reads.
    EXPECT_FALSE(hasOptionsConstructor<DeflateStream>);
    EXPECT_FALSE(hasOptionsConstructor<GZipStream>);
    EXPECT_FALSE(hasOptionsConstructor<ZLibStream>);
    EXPECT_TRUE((std::is_constructible_v<DeflateStream, System::IO::Stream*, CompressionMode, bool>));
}

// ---------------------------------------------------------------------------
// 2. The documented raw-pointer contract, asserted at the doors the headers name
// ---------------------------------------------------------------------------

namespace {

    template <typename Encoder>
    void expectEncoderContract(const char* label) {
        bytecs src[4] = {bytecs{'a'}, bytecs{'b'}, bytecs{'c'}, bytecs{'d'}};
        bytecs dst[64] = {};
        intcs consumed = 99, written = 99;
        Encoder e;

        EXPECT_THROW(e.Compress(src, -1, dst, 64, consumed, written, true),
                     System::ArgumentOutOfRangeException) << label;
        EXPECT_THROW(e.Compress(src, 4, dst, -1, consumed, written, true),
                     System::ArgumentOutOfRangeException) << label;
        EXPECT_THROW(e.Compress(nullptr, 4, dst, 64, consumed, written, true),
                     System::ArgumentNullException) << label;
        EXPECT_THROW(e.Compress(src, 4, nullptr, 64, consumed, written, true),
                     System::ArgumentNullException) << label;
        EXPECT_THROW(e.Flush(dst, -1, written), System::ArgumentOutOfRangeException) << label;

        // The out-parameters must be untouched by every rejected call above.
        EXPECT_EQ(consumed, 99) << label;
        EXPECT_EQ(written, 99) << label;

        // A null buffer with a length of zero is legal, exactly as the header says.
        intcs c2 = 0, w2 = 0;
        EXPECT_NO_THROW((void)e.Compress(nullptr, 0, dst, 64, c2, w2, false)) << label;

        // The static Try* door carries the same contract: it THROWS, it does not report false.
        intcs w3 = 0;
        EXPECT_THROW((void)Encoder::TryCompress(src, -1, dst, 64, w3),
                     System::ArgumentOutOfRangeException) << label;
    }

    template <typename Decoder>
    void expectDecoderContract(const char* label) {
        bytecs src[4] = {};
        bytecs dst[64] = {};
        intcs consumed = 99, written = 99;
        Decoder d;

        EXPECT_THROW(d.Decompress(src, -1, dst, 64, consumed, written),
                     System::ArgumentOutOfRangeException) << label;
        EXPECT_THROW(d.Decompress(src, 4, dst, -1, consumed, written),
                     System::ArgumentOutOfRangeException) << label;
        EXPECT_THROW(d.Decompress(nullptr, 4, dst, 64, consumed, written),
                     System::ArgumentNullException) << label;
        EXPECT_THROW(d.Decompress(src, 4, nullptr, 64, consumed, written),
                     System::ArgumentNullException) << label;
        EXPECT_EQ(consumed, 99) << label;
        EXPECT_EQ(written, 99) << label;

        intcs w3 = 0;
        EXPECT_THROW((void)Decoder::TryDecompress(src, -1, dst, 64, w3),
                     System::ArgumentOutOfRangeException) << label;
    }

} // namespace

TEST(CompressionContractPinTests, EveryEncoderDoorMatchesItsDocumentedContract) {
    expectEncoderContract<DeflateEncoder>("DeflateEncoder");
    expectEncoderContract<GZipEncoder>("GZipEncoder");
    expectEncoderContract<ZLibEncoder>("ZLibEncoder");
}

TEST(CompressionContractPinTests, EveryDecoderDoorMatchesItsDocumentedContract) {
    expectDecoderContract<DeflateDecoder>("DeflateDecoder");
    expectDecoderContract<GZipDecoder>("GZipDecoder");
    expectDecoderContract<ZLibDecoder>("ZLibDecoder");
}
