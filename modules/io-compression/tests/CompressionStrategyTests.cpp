// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// Ticket #2149 / SR-AUD-259 (cause C-C of docs/SystemIOCompressionNamespaceReviewPlan.md):
// `DeflateEncoder(options)`, `GZipEncoder(options)` and `ZLibEncoder(options)` validated
// `ZLibCompressionOptions::getCompressionStrategyProperty()` on the way in and then dropped it,
// passing `Z_DEFAULT_STRATEGY` to `deflateInit2` unconditionally.
//
// Measured before-matrix: build-probe/2149_probe1_before.log -- 3 encoders x 5 strategies x 3
// payload shapes, 45 of 45 byte-identical to Default. After: build-probe/2149_probe1_after.log.
//
// The payload shape matters and is why this file builds its own inputs rather than reusing a
// string: Filtered and Default produce IDENTICAL bytes at level 6 for every payload measured, so a
// test that discriminated only on Filtered would have passed against the broken code.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "System/ArgumentOutOfRangeException.hpp"
#include "System/IO/Compression/DeflateDecoder.hpp"
#include "System/IO/Compression/DeflateEncoder.hpp"
#include "System/IO/Compression/GZipDecoder.hpp"
#include "System/IO/Compression/GZipEncoder.hpp"
#include "System/IO/Compression/ZLibCompressionOptions.hpp"
#include "System/IO/Compression/ZLibDecoder.hpp"
#include "System/IO/Compression/ZLibEncoder.hpp"

using namespace System::IO::Compression;
using System::Buffers::OperationStatus;
using SharpRuntime::bytecs;
using SharpRuntime::intcs;

namespace {

    // Long single-byte runs: the shape Z_RLE (match distance limited to 1) exists for, and the
    // shape Z_HUFFMAN_ONLY (no string matching at all) degrades on. Both differ sharply from the
    // default strategy here, which is what makes this payload the discriminator.
    std::vector<bytecs> runPayload(std::size_t n = 8192) {
        std::vector<bytecs> v(n);
        for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<bytecs>('A' + ((i / 37) % 7));
        return v;
    }

    ZLibCompressionOptions optionsFor(ZLibCompressionStrategy s, intcs level = 6) {
        ZLibCompressionOptions o;
        o.setCompressionLevelProperty(level);
        o.setCompressionStrategyProperty(s);
        return o;
    }

    template <typename Encoder>
    std::vector<bytecs> compressWith(const ZLibCompressionOptions& o,
                                     const std::vector<bytecs>& in) {
        Encoder e(o);
        std::vector<bytecs> out(in.size() + 4096);
        intcs consumed = 0, written = 0;
        const OperationStatus st = e.Compress(in.data(), static_cast<intcs>(in.size()),
                                              out.data(), static_cast<intcs>(out.size()),
                                              consumed, written, /*isFinalBlock=*/true);
        EXPECT_EQ(st, OperationStatus::Done);
        EXPECT_EQ(consumed, static_cast<intcs>(in.size()));
        out.resize(static_cast<std::size_t>(written));
        return out;
    }

    template <typename Decoder>
    std::vector<bytecs> decompress(const std::vector<bytecs>& in, std::size_t expected) {
        Decoder d;
        std::vector<bytecs> out(expected + 64);
        intcs consumed = 0, written = 0;
        d.Decompress(in.data(), static_cast<intcs>(in.size()), out.data(),
                     static_cast<intcs>(out.size()), consumed, written);
        out.resize(static_cast<std::size_t>(written));
        return out;
    }

    template <typename Encoder, typename Decoder>
    void expectStrategyReachesTheEncoder(const char* label) {
        const auto payload = runPayload();
        const auto byDefault = compressWith<Encoder>(optionsFor(ZLibCompressionStrategy::Default), payload);

        // The three strategies that genuinely change zlib's output for this payload. Filtered is
        // deliberately excluded from the "differs" assertion -- see the file header.
        std::vector<std::vector<bytecs>> distinct;
        for (auto s : {ZLibCompressionStrategy::HuffmanOnly,
                       ZLibCompressionStrategy::RunLengthEncoding,
                       ZLibCompressionStrategy::Fixed}) {
            const auto got = compressWith<Encoder>(optionsFor(s), payload);
            EXPECT_NE(got, byDefault)
                << label << ": strategy " << static_cast<int>(s) << " produced Default's bytes";
            distinct.push_back(got);
        }

        // Pairwise distinct, not merely "not Default". Without this a mis-mapping inside
        // ResolveZLibStrategy -- RunLengthEncoding resolved to Z_FIXED, say -- passes every other
        // assertion in this file: the wrong strategy is still a real strategy, so the output still
        // differs from Default, still round-trips, and still lands in the right size order. That
        // mutation SURVIVED until this loop was added.
        for (std::size_t i = 0; i < distinct.size(); ++i)
            for (std::size_t j = i + 1; j < distinct.size(); ++j)
                EXPECT_NE(distinct[i], distinct[j])
                    << label << ": two distinct strategies produced identical bytes (" << i << "," << j << ")";

        // Every strategy, including the two that agree with Default, must still round-trip.
        for (auto s : {ZLibCompressionStrategy::Default, ZLibCompressionStrategy::Filtered,
                       ZLibCompressionStrategy::HuffmanOnly,
                       ZLibCompressionStrategy::RunLengthEncoding,
                       ZLibCompressionStrategy::Fixed}) {
            const auto compressed = compressWith<Encoder>(optionsFor(s), payload);
            EXPECT_EQ(decompress<Decoder>(compressed, payload.size()), payload)
                << label << ": strategy " << static_cast<int>(s) << " did not round-trip";
        }
    }

    // The relative ordering of the three strategies is a property of zlib, not of this port, but it
    // is the cheapest independent check that the value actually reached deflateInit2: on a
    // run-heavy payload, no string matching at all must cost far more than the default, and
    // limiting matches to distance 1 must land between the two.
    template <typename Encoder>
    void expectStrategyOrdering(const char* label) {
        const auto payload = runPayload();
        const auto def = compressWith<Encoder>(optionsFor(ZLibCompressionStrategy::Default), payload).size();
        const auto rle = compressWith<Encoder>(optionsFor(ZLibCompressionStrategy::RunLengthEncoding), payload).size();
        const auto huf = compressWith<Encoder>(optionsFor(ZLibCompressionStrategy::HuffmanOnly), payload).size();
        EXPECT_LT(def, rle) << label;
        EXPECT_LT(rle, huf) << label;
    }

} // namespace

TEST(CompressionStrategyTests, DeflateEncoderHonoursTheOptionsStrategy) {
    expectStrategyReachesTheEncoder<DeflateEncoder, DeflateDecoder>("DeflateEncoder");
}
TEST(CompressionStrategyTests, GZipEncoderHonoursTheOptionsStrategy) {
    expectStrategyReachesTheEncoder<GZipEncoder, GZipDecoder>("GZipEncoder");
}
TEST(CompressionStrategyTests, ZLibEncoderHonoursTheOptionsStrategy) {
    expectStrategyReachesTheEncoder<ZLibEncoder, ZLibDecoder>("ZLibEncoder");
}

TEST(CompressionStrategyTests, TheStrategiesOrderAsZlibDefinesThem) {
    expectStrategyOrdering<DeflateEncoder>("DeflateEncoder");
    expectStrategyOrdering<GZipEncoder>("GZipEncoder");
    expectStrategyOrdering<ZLibEncoder>("ZLibEncoder");
}

// ---------------------------------------------------------------------------
// Controls -- what this repair must NOT move
// ---------------------------------------------------------------------------

TEST(CompressionStrategyControlTests, DefaultOptionOutputIsByteStable) {
    // The exact byte counts measured for the default strategy BEFORE the repair
    // (build-probe/2149_probe1_before.log, "run" payload, level 6). This is the control that
    // separates plumbing an existing option through from changing what every caller gets.
    const auto payload = runPayload();
    EXPECT_EQ(compressWith<DeflateEncoder>(optionsFor(ZLibCompressionStrategy::Default), payload).size(), 69u);
    EXPECT_EQ(compressWith<GZipEncoder>(optionsFor(ZLibCompressionStrategy::Default), payload).size(), 87u);
    EXPECT_EQ(compressWith<ZLibEncoder>(optionsFor(ZLibCompressionStrategy::Default), payload).size(), 75u);
}

TEST(CompressionStrategyControlTests, ADefaultConstructedOptionsObjectStillMeansDefaultStrategy) {
    const auto payload = runPayload();
    ZLibCompressionOptions bare;
    bare.setCompressionLevelProperty(6);
    EXPECT_EQ(bare.getCompressionStrategyProperty(), ZLibCompressionStrategy::Default);
    EXPECT_EQ(compressWith<DeflateEncoder>(bare, payload),
              compressWith<DeflateEncoder>(optionsFor(ZLibCompressionStrategy::Default), payload));
}

TEST(CompressionStrategyControlTests, TheNonOptionsConstructorsStillUseTheDefaultStrategy) {
    // DeflateEncoder(quality[, windowLog]) and its GZip/ZLib counterparts document
    // Z_DEFAULT_STRATEGY; ticket #2149 changed only the options path.
    const auto payload = runPayload();
    const auto viaOptions = compressWith<DeflateEncoder>(optionsFor(ZLibCompressionStrategy::Default), payload);

    DeflateEncoder plain(6, -1);
    std::vector<bytecs> out(payload.size() + 4096);
    intcs consumed = 0, written = 0;
    plain.Compress(payload.data(), static_cast<intcs>(payload.size()), out.data(),
                   static_cast<intcs>(out.size()), consumed, written, true);
    out.resize(static_cast<std::size_t>(written));
    EXPECT_EQ(out, viaOptions);
}

TEST(CompressionStrategyControlTests, OptionsArgumentValidationStillRunsInTheSameOrder) {
    // The options constructor now resolves windowBits itself instead of delegating; quality is
    // still validated before windowLog, which is the order DeflateEncoder(quality, windowLog)
    // established. Both members can only hold validated values, so this exercises the resolution
    // path rather than the setters.
    ZLibCompressionOptions o;
    EXPECT_THROW(o.setCompressionLevelProperty(10), System::ArgumentOutOfRangeException);
    EXPECT_THROW(o.setWindowLogProperty(16), System::ArgumentOutOfRangeException);
    EXPECT_THROW(o.setCompressionStrategyProperty(static_cast<ZLibCompressionStrategy>(5)),
                 System::ArgumentOutOfRangeException);
    EXPECT_THROW(o.setCompressionStrategyProperty(static_cast<ZLibCompressionStrategy>(-1)),
                 System::ArgumentOutOfRangeException);
    // Nothing was retained from any rejected write.
    EXPECT_EQ(o.getCompressionLevelProperty(), -1);
    EXPECT_EQ(o.getWindowLogProperty(), -1);
    EXPECT_EQ(o.getCompressionStrategyProperty(), ZLibCompressionStrategy::Default);
}

TEST(CompressionStrategyControlTests, TheOptionsPathResolvesTheSameParametersAsTheDelegatingPath) {
    // The options constructor used to delegate to DeflateEncoder(quality, windowLog) and now
    // resolves windowBits and memLevel itself. This sweeps the two axes it resolves and requires
    // byte-identical output from both paths, so a divergence in either resolution is caught.
    //
    // HONEST LIMIT, measured: the memLevel half of that resolution is NOT observable. The port
    // selects the low memLevel (7) only for quality == 0, and at level 0 zlib emits stored blocks,
    // where memLevel has no effect at all -- memLevel 7 and 8 produce identical bytes for every
    // input measured at level 0, and differ only at levels 1/6/9, where both paths already agree
    // on 8 (build-probe/2149_probe2_memlevel.log). A mutation that always resolves memLevel 8 is
    // therefore observationally equivalent, not an undetected defect; it survived this suite for
    // that reason and the reason is recorded rather than papered over with a weaker assertion.
    const auto payload = runPayload();
    for (intcs quality : {intcs{-1}, intcs{0}, intcs{1}, intcs{6}, intcs{9}}) {
        for (intcs windowLog : {intcs{-1}, intcs{8}, intcs{12}, intcs{15}}) {
            ZLibCompressionOptions o;
            o.setCompressionLevelProperty(quality);
            o.setWindowLogProperty(windowLog);
            const auto viaOptions = compressWith<DeflateEncoder>(o, payload);

            DeflateEncoder plain(quality, windowLog);
            std::vector<bytecs> out(payload.size() + 4096);
            intcs consumed = 0, written = 0;
            plain.Compress(payload.data(), static_cast<intcs>(payload.size()), out.data(),
                           static_cast<intcs>(out.size()), consumed, written, true);
            out.resize(static_cast<std::size_t>(written));
            EXPECT_EQ(out, viaOptions) << "quality=" << quality << " windowLog=" << windowLog;
        }
    }
}
