// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
//
// SR-AUD-175 (ticket #2262): float -> BFloat16 conversion must round to nearest
// with ties to even, matching current .NET, which passes non-NaN float bits
// through RoundMidpointToEven(bits, 16) before taking the upper half.
//
// Before the repair fromFloat() was a bare `u >> 16`, which truncated. Every
// case below is stated as an exact 32-bit float bit pattern, so nothing here
// depends on how a decimal literal is parsed or on the host's rounding mode.
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include "System/Numerics/BFloat16.hpp"

using System::Numerics::BFloat16;

namespace {

    float asFloat(std::uint32_t bits) {
        float f;
        std::memcpy(&f, &bits, 4);
        return f;
    }

    std::uint16_t convert(std::uint32_t floatBits) {
        return BFloat16(asFloat(floatBits)).getBitsProperty();
    }

} // namespace

// ---------------------------------------------------------------------------
// The finding's own two reproductions
// ---------------------------------------------------------------------------

TEST(BFloat16RoundingTests, ExactMidpointAboveOddPayload_TiesToEven) {
    // 0x3F818000 is the exact midpoint above odd payload 0x3F81; ties round to
    // the even neighbour 0x3F82. Truncation returned the lower payload 0x3F81.
    EXPECT_EQ(convert(0x3F818000u), std::uint16_t(0x3F82u));
}

TEST(BFloat16RoundingTests, JustAboveMidpoint_RoundsUp) {
    // 0x3F808001 is strictly above the midpoint from 0x3F80; it must round up.
    EXPECT_EQ(convert(0x3F808001u), std::uint16_t(0x3F81u));
}

// ---------------------------------------------------------------------------
// The other half of ties-to-even, and the ordinary directed cases
// ---------------------------------------------------------------------------

TEST(BFloat16RoundingTests, ExactMidpointAboveEvenPayload_StaysEven) {
    EXPECT_EQ(convert(0x3F808000u), std::uint16_t(0x3F80u));
}

TEST(BFloat16RoundingTests, JustBelowMidpoint_RoundsDown) {
    EXPECT_EQ(convert(0x3F817FFFu), std::uint16_t(0x3F81u));
}

TEST(BFloat16RoundingTests, RoundingIsSignSymmetric) {
    EXPECT_EQ(convert(0xBF818000u), std::uint16_t(0xBF82u));
    EXPECT_EQ(convert(0xBF808001u), std::uint16_t(0xBF81u));
}

TEST(BFloat16RoundingTests, CarryOutOfMantissaEntersTheExponent) {
    // Every retained mantissa bit set, at the midpoint: rounding up must carry
    // out of the mantissa and increment the exponent rather than wrap.
    EXPECT_EQ(convert(0x3FFF8000u), std::uint16_t(0x4000u));
}

// ---------------------------------------------------------------------------
// Exactly representable inputs -- the compatibility control. These have no
// discarded bits, so truncation and rounding agree and nothing changed here.
// ---------------------------------------------------------------------------

TEST(BFloat16RoundingTests, ExactlyRepresentableValuesAreUnchanged) {
    EXPECT_EQ(convert(0x00000000u), std::uint16_t(0x0000u));  // +0
    EXPECT_EQ(convert(0x80000000u), std::uint16_t(0x8000u));  // -0
    EXPECT_EQ(convert(0x3F800000u), std::uint16_t(0x3F80u));  // 1.0
    EXPECT_EQ(convert(0xBF800000u), std::uint16_t(0xBF80u));  // -1.0
    EXPECT_EQ(convert(0x40000000u), std::uint16_t(0x4000u));  // 2.0
    EXPECT_EQ(convert(0x40400000u), std::uint16_t(0x4040u));  // 3.0
}

TEST(BFloat16RoundingTests, SignedZeroKeepsItsSign) {
    EXPECT_EQ(convert(0x80000000u), std::uint16_t(0x8000u));
    EXPECT_TRUE(BFloat16(asFloat(0x80000000u)) == BFloat16(asFloat(0x00000000u)));
}

// ---------------------------------------------------------------------------
// Overflow and infinity
// ---------------------------------------------------------------------------

TEST(BFloat16RoundingTests, FiniteValueAboveMaxPlusHalfUlp_RoundsToInfinity) {
    // Ordinary IEEE overflow-on-rounding, not a defect: float MaxValue exceeds
    // BFloat16 MaxValue + half ulp, and the exact tie at that midpoint rounds to
    // the even neighbour, which is the infinity pattern.
    EXPECT_EQ(convert(0x7F7FFFFFu), std::uint16_t(0x7F80u));
    EXPECT_EQ(convert(0xFF7FFFFFu), std::uint16_t(0xFF80u));
    EXPECT_EQ(convert(0x7F7F8000u), std::uint16_t(0x7F80u));
}

TEST(BFloat16RoundingTests, InfinitiesPassThrough) {
    EXPECT_EQ(convert(0x7F800000u), std::uint16_t(0x7F80u));
    EXPECT_EQ(convert(0xFF800000u), std::uint16_t(0xFF80u));
    EXPECT_TRUE(BFloat16::IsPositiveInfinity(BFloat16(asFloat(0x7F800000u))));
    EXPECT_TRUE(BFloat16::IsNegativeInfinity(BFloat16(asFloat(0xFF800000u))));
}

// ---------------------------------------------------------------------------
// NaN. This is the sharpest consequence of the old truncation and the finding
// never named it: 0x7F800001 is a signalling NaN whose upper 16 bits are
// exactly the +Infinity pattern, so `u >> 16` turned a NaN into an infinity.
// Rounding cannot be applied to a NaN either -- adding the bias can carry into
// the exponent and produce the same silent infinity -- so NaN is handled first.
// ---------------------------------------------------------------------------

TEST(BFloat16RoundingTests, SignallingNaNStaysNaNAndNeverBecomesInfinity) {
    const BFloat16 positive(asFloat(0x7F800001u));
    EXPECT_EQ(positive.getBitsProperty(), std::uint16_t(0x7FC0u));
    EXPECT_TRUE(BFloat16::IsNaN(positive));
    EXPECT_FALSE(BFloat16::IsInfinity(positive));

    const BFloat16 negative(asFloat(0xFF800001u));
    EXPECT_EQ(negative.getBitsProperty(), std::uint16_t(0xFFC0u));
    EXPECT_TRUE(BFloat16::IsNaN(negative));
    EXPECT_FALSE(BFloat16::IsInfinity(negative));
}

TEST(BFloat16RoundingTests, QuietNaNKeepsItsSignAndStaysQuiet) {
    EXPECT_EQ(convert(0x7FC00000u), std::uint16_t(0x7FC0u));
    EXPECT_EQ(convert(0xFFC00000u), std::uint16_t(0xFFC0u));
    EXPECT_EQ(convert(0xFFC00000u), BFloat16::NaN().getBitsProperty());
}

TEST(BFloat16RoundingTests, EveryNaNPayloadStaysNaN) {
    // Sweep the mantissa: no NaN input may leave the conversion as an infinity
    // or as a finite value, whichever bits carry the payload.
    for (std::uint32_t mantissa = 1; mantissa < 0x00800000u; mantissa <<= 1) {
        for (std::uint32_t sign : {0x00000000u, 0x80000000u}) {
            const BFloat16 v(asFloat(sign | 0x7F800000u | mantissa));
            EXPECT_TRUE(BFloat16::IsNaN(v))
                << "mantissa=0x" << std::hex << mantissa << " sign=0x" << sign;
        }
    }
}

// ---------------------------------------------------------------------------
// Underflow
// ---------------------------------------------------------------------------

TEST(BFloat16RoundingTests, UnderflowRoundsTiesToEvenZero) {
    EXPECT_EQ(convert(0x00000001u), std::uint16_t(0x0000u));  // far below half ulp
    EXPECT_EQ(convert(0x00008000u), std::uint16_t(0x0000u));  // exact tie -> even 0
    EXPECT_EQ(convert(0x00008001u), std::uint16_t(0x0001u));  // just above -> Epsilon
    EXPECT_EQ(convert(0x00018000u), std::uint16_t(0x0002u));  // tie above odd -> even
}

// ---------------------------------------------------------------------------
// Arithmetic constructs through the same conversion, which is why the finding
// calls the bias systematic rather than local.
// ---------------------------------------------------------------------------

TEST(BFloat16RoundingTests, ArithmeticResultsRoundRatherThanTruncate) {
    // Doubling a BFloat16 is always exact -- it only increments the exponent --
    // so the sum has to be of two DIFFERENT payloads to leave discarded bits.
    // 0x3F81 (1.0078125) + 0x3F82 (1.015625) is 2.0234375, whose float bits are
    // 0x40018000: the exact midpoint above odd payload 0x4001, so the sum must
    // round to even 0x4002. Truncation returned 0x4001.
    const BFloat16 a(std::uint16_t(0x3F81u));
    const BFloat16 b(std::uint16_t(0x3F82u));
    EXPECT_EQ((a + b).getBitsProperty(), std::uint16_t(0x4002u));

    // The same sum reached the other way round must agree.
    EXPECT_EQ((b + a).getBitsProperty(), std::uint16_t(0x4002u));
}

TEST(BFloat16RoundingTests, ExactArithmeticIsStillExact) {
    EXPECT_EQ((BFloat16(1.0f) + BFloat16(2.0f)).getBitsProperty(),
              BFloat16(3.0f).getBitsProperty());
    EXPECT_EQ((BFloat16(2.0f) * BFloat16(2.0f)).getBitsProperty(),
              BFloat16(4.0f).getBitsProperty());
}

// ---------------------------------------------------------------------------
// Representation: the repair is confined to a private static conversion.
// ---------------------------------------------------------------------------

TEST(BFloat16RoundingTests, LayoutIsUnchanged) {
    EXPECT_EQ(sizeof(BFloat16), 2u);
    EXPECT_EQ(alignof(BFloat16), 2u);
}
