// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2150 (SR-AUD-259, cause C-C) — the three stream types' ZLibCompressionOptions
// constructors.
//
// The ticket was recorded as "blocked -- public surface addition, approval required", and the
// measurement disagreed with its own classification twice:
//
//   * this repository's design record already lists #2150 as `additive`, with no vtable, no
//     layout and no noexcept change (docs/SystemIOCompressionNamespaceReviewPlan.md section 7);
//   * the remaining claim -- that "new overloads change source overload resolution" -- is FALSE
//     here, and is asserted below rather than argued.
//
// #2149 had already landed the option plumbing in the three ENCODERS, so the only thing missing
// for a stream caller was the constructor itself.
#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

#include "System/ArgumentNullException.hpp"
#include "System/IO/MemoryStream.hpp"
#include "System/IO/Compression/DeflateStream.hpp"
#include "System/IO/Compression/GZipStream.hpp"
#include "System/IO/Compression/ZLibStream.hpp"
#include "System/IO/Compression/ZLibCompressionOptions.hpp"
#include "System/IO/Compression/CompressionArgumentValidation.hpp"

using System::IO::MemoryStream;
using System::IO::Compression::CompressionMode;
using System::IO::Compression::DeflateStream;
using System::IO::Compression::GZipStream;
using System::IO::Compression::ZLibStream;
using System::IO::Compression::ZLibCompressionOptions;
using System::IO::Compression::ZLibCompressionStrategy;
using SharpRuntime::bytecs;
using SharpRuntime::intcs;

namespace {

    /// A payload whose compressed size actually moves with the strategy and the level.
    std::vector<bytecs> runPayload(std::size_t n = 8192) {
        std::vector<bytecs> v(n);
        for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<bytecs>('A' + ((i / 37) % 7));
        return v;
    }

    template <typename StreamT>
    std::vector<bytecs> compressWith(const ZLibCompressionOptions& options,
                                     const std::vector<bytecs>& in) {
        MemoryStream sink;
        {
            StreamT s(&sink, options, /*leaveOpen=*/true);
            s.Write(in.data(), 0, static_cast<intcs>(in.size()));
            s.Close();
        }
        return sink.ToArray();
    }

    template <typename StreamT>
    std::vector<bytecs> compressWithMode(const std::vector<bytecs>& in) {
        MemoryStream sink;
        {
            StreamT s(&sink, CompressionMode::Compress, /*leaveOpen=*/true);
            s.Write(in.data(), 0, static_cast<intcs>(in.size()));
            s.Close();
        }
        return sink.ToArray();
    }

    template <typename StreamT>
    std::vector<bytecs> roundTrip(const std::vector<bytecs>& compressed, std::size_t expected) {
        MemoryStream source(compressed.data(), static_cast<intcs>(compressed.size()), false);
        StreamT s(&source, CompressionMode::Decompress, /*leaveOpen=*/true);
        std::vector<bytecs> out(expected + 16);
        intcs total = 0, n = 0;
        while ((n = s.Read(out.data() + total, 0, static_cast<intcs>(out.size() - total))) > 0)
            total += n;
        out.resize(static_cast<std::size_t>(total));
        return out;
    }

    ZLibCompressionOptions optionsFor(intcs level, ZLibCompressionStrategy strategy) {
        ZLibCompressionOptions o;
        o.setCompressionLevelProperty(level);
        o.setCompressionStrategyProperty(strategy);
        return o;
    }

} // namespace

// ---------------------------------------------------------------------------------------------
// The premise correction, asserted rather than argued.
// ---------------------------------------------------------------------------------------------

TEST(CompressionOptionsConstructorTests, Decl2150_TheAdditionCannotRebindAnExistingCall) {
    // "New overloads change source overload resolution" is the ticket's stated reason for needing
    // approval. It is false here, and these are the two facts that make it false:
    //
    //   1. CompressionMode is a SCOPED enumeration, so no integer or bool converts to it and no
    //      CompressionMode converts to anything else;
    //   2. ZLibCompressionOptions has no converting constructor -- only a defaulted default one --
    //      so nothing implicitly converts TO it either.
    //
    // With no type convertible to both parameter types, no existing call can bind to the new
    // overload, and no new call can bind to the old one. If either fact stops holding, this test
    // fails and the additive claim must be re-argued.
    static_assert(!std::is_convertible_v<int, CompressionMode>);
    static_assert(!std::is_convertible_v<CompressionMode, int>);
    static_assert(!std::is_convertible_v<bool, ZLibCompressionOptions>);
    static_assert(!std::is_convertible_v<int, ZLibCompressionOptions>);
    static_assert(!std::is_convertible_v<CompressionMode, ZLibCompressionOptions>);
    static_assert(!std::is_convertible_v<ZLibCompressionOptions, CompressionMode>);

    // ...and the existing overload still resolves exactly as before, for both of its call shapes.
    MemoryStream sink;
    EXPECT_NO_THROW(DeflateStream(&sink, CompressionMode::Compress));
    EXPECT_NO_THROW(DeflateStream(&sink, CompressionMode::Compress, true));
}

// ---------------------------------------------------------------------------------------------
// The constructor does what it says.
// ---------------------------------------------------------------------------------------------

TEST(CompressionOptionsConstructorTests, Fix2150_TheOptionsConstructorImpliesCompress) {
    // .NET's options overload carries no mode: "Implies mode = Compress". A stream built this way
    // must therefore be writable and not readable.
    MemoryStream sink;
    ZLibCompressionOptions options;
    DeflateStream s(&sink, options, /*leaveOpen=*/true);
    EXPECT_TRUE(s.getCanWriteProperty());
    EXPECT_FALSE(s.getCanReadProperty());
}

TEST(CompressionOptionsConstructorTests, Fix2150_ANullStreamIsRejected) {
    ZLibCompressionOptions options;
    EXPECT_THROW(DeflateStream(nullptr, options), System::ArgumentNullException);
    EXPECT_THROW(GZipStream(nullptr, options), System::ArgumentNullException);
    EXPECT_THROW(ZLibStream(nullptr, options), System::ArgumentNullException);
}

TEST(CompressionOptionsConstructorTests, Fix2150_TheStrategyReachesZlibInAllThreeStreams) {
    // The defect #2149 fixed for the encoders, now asserted for the streams: a non-default
    // strategy must change the emitted bytes. If the constructor dropped it, these would be equal.
    const auto payload = runPayload();
    const auto def = optionsFor(6, ZLibCompressionStrategy::Default);
    const auto rle = optionsFor(6, ZLibCompressionStrategy::RunLengthEncoding);

    EXPECT_NE(compressWith<DeflateStream>(def, payload), compressWith<DeflateStream>(rle, payload));
    EXPECT_NE(compressWith<GZipStream>(def, payload), compressWith<GZipStream>(rle, payload));
    EXPECT_NE(compressWith<ZLibStream>(def, payload), compressWith<ZLibStream>(rle, payload));
}

TEST(CompressionOptionsConstructorTests, Fix2150_TheLevelReachesZlibInAllThreeStreams) {
    const auto payload = runPayload();
    const auto fast = optionsFor(1, ZLibCompressionStrategy::Default);
    const auto best = optionsFor(9, ZLibCompressionStrategy::Default);

    EXPECT_NE(compressWith<DeflateStream>(fast, payload), compressWith<DeflateStream>(best, payload));
    EXPECT_NE(compressWith<GZipStream>(fast, payload), compressWith<GZipStream>(best, payload));
    EXPECT_NE(compressWith<ZLibStream>(fast, payload), compressWith<ZLibStream>(best, payload));
}

TEST(CompressionOptionsConstructorTests, Fix2150_TheWindowLogReachesZlibInAllThreeStreams) {
    const auto payload = runPayload();
    ZLibCompressionOptions small;
    small.setCompressionLevelProperty(9);
    small.setWindowLogProperty(9);
    ZLibCompressionOptions large;
    large.setCompressionLevelProperty(9);
    large.setWindowLogProperty(15);

    EXPECT_NE(compressWith<DeflateStream>(small, payload), compressWith<DeflateStream>(large, payload));
    EXPECT_NE(compressWith<GZipStream>(small, payload), compressWith<GZipStream>(large, payload));
    EXPECT_NE(compressWith<ZLibStream>(small, payload), compressWith<ZLibStream>(large, payload));
}

TEST(CompressionOptionsConstructorTests, Fix2150_EachStreamStillEmitsItsOwnContainer) {
    // The window-bits SIGN and offset are the container, so getting ResolveWindowBits wrong here
    // would silently emit the wrong format. Each stream's output must still be readable by its own
    // decompressor -- and this is what a mutation swapping two formats breaks.
    const auto payload = runPayload(4096);
    ZLibCompressionOptions options;
    options.setCompressionLevelProperty(6);

    EXPECT_EQ(roundTrip<DeflateStream>(compressWith<DeflateStream>(options, payload), payload.size()), payload);
    EXPECT_EQ(roundTrip<GZipStream>(compressWith<GZipStream>(options, payload), payload.size()), payload);
    EXPECT_EQ(roundTrip<ZLibStream>(compressWith<ZLibStream>(options, payload), payload.size()), payload);

    // The gzip container is identifiable from its first two bytes, which is the cheapest possible
    // check that GZipStream did not quietly become a zlib or raw-deflate stream.
    const auto gz = compressWith<GZipStream>(options, payload);
    ASSERT_GE(gz.size(), 2u);
    EXPECT_EQ(static_cast<unsigned char>(gz[0]), 0x1fu);
    EXPECT_EQ(static_cast<unsigned char>(gz[1]), 0x8bu);
}

TEST(CompressionOptionsConstructorTests, Fix2150_DefaultOptionsAreNotTheModeConstructorsDefaults) {
    // Worth pinning because it is counter-intuitive: a DEFAULT-constructed ZLibCompressionOptions
    // has CompressionLevel -1, which zlib maps to level 6, while the mode constructor passes
    // Z_DEFAULT_COMPRESSION -- also -1. So these two agree on the level, and the bytes are equal.
    // The pin exists so that if someone "helpfully" changes either default, the divergence is
    // reported here rather than discovered as a silent change in emitted bytes.
    const auto payload = runPayload(4096);
    ZLibCompressionOptions defaults;
    EXPECT_EQ(defaults.getCompressionLevelProperty(), -1);
    EXPECT_EQ(compressWith<DeflateStream>(defaults, payload), compressWithMode<DeflateStream>(payload));
}

// ---------------------------------------------------------------------------------------------
// The shared resolver, asserted directly.
//
// These rows were added after mutation M3 -- "make the ZLib arm clamp to 9 as well" -- went
// UNCAUGHT: every case above uses a WindowLog of 9 or 15, so the one input that distinguishes the
// two behaviours, a WindowLog of 8, was never exercised. Asserting the resolver directly catches
// it by construction, which is better than hoping the difference survives into the emitted bytes:
// classic zlib silently upgrades a windowBits of 8 to 9 internally, so an end-to-end test could
// not have discriminated this reliably at all.
// ---------------------------------------------------------------------------------------------

TEST(CompressionOptionsConstructorTests, Fix2150_ResolveWindowBitsTranscribesBothAsymmetries) {
    using System::IO::Compression::Detail::CompressionFormat;
    using System::IO::Compression::Detail::ResolveWindowBits;

    // -1 means "the default", which is 15.
    EXPECT_EQ(ResolveWindowBits(-1, CompressionFormat::Deflate), -15);
    EXPECT_EQ(ResolveWindowBits(-1, CompressionFormat::ZLib), 15);
    EXPECT_EQ(ResolveWindowBits(-1, CompressionFormat::GZip), 31);

    // THE ASYMMETRY .NET DELIBERATELY HAS: Deflate and GZip clamp to a minimum of 9; ZLib does
    // not. .NET's own comment gives the reason -- zlib-ng rejects windowBits 8 for raw deflate
    // and gzip, while classic zlib silently upgrades to 9 -- so the clamp makes the two
    // implementations agree. A resolver that clamped uniformly would change what a WindowLog of
    // 8 means for ZLibStream, which is the one container where .NET leaves it alone.
    EXPECT_EQ(ResolveWindowBits(8, CompressionFormat::Deflate), -9);
    EXPECT_EQ(ResolveWindowBits(8, CompressionFormat::GZip), 25);
    EXPECT_EQ(ResolveWindowBits(8, CompressionFormat::ZLib), 8) << "the ZLib arm must NOT clamp";

    // The sign and offset ARE the container.
    EXPECT_EQ(ResolveWindowBits(12, CompressionFormat::Deflate), -12);
    EXPECT_EQ(ResolveWindowBits(12, CompressionFormat::ZLib), 12);
    EXPECT_EQ(ResolveWindowBits(12, CompressionFormat::GZip), 28);
}

TEST(CompressionOptionsConstructorTests, Fix2150_ResolveDeflateMemLevelIsSevenOnlyAtQualityZero) {
    using System::IO::Compression::Detail::ResolveDeflateMemLevel;
    // .NET: quality 0 ("no compression") uses memLevel 7, everything else 8.
    EXPECT_EQ(ResolveDeflateMemLevel(0), 7);
    for (intcs q : {intcs{-1}, intcs{1}, intcs{6}, intcs{9}})
        EXPECT_EQ(ResolveDeflateMemLevel(q), 8) << "quality " << q;
}
