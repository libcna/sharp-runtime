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
#include <bit>
#include <gtest/gtest.h>
#include "System/Decimal.hpp"
#include "System/ArithmeticException.hpp"
#include "System/Half.hpp"
#include <array>
#include <limits>
#include <string>
#include <type_traits>
#include "System/FormatException.hpp"
#include "System/Numerics/BFloat16.hpp"
#include "System/Single.hpp"
#include "System/Span.hpp"

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

    template<typename TInteger, typename TFloat16>
    void expectDefinedIntegralEdges(TFloat16 nan, TFloat16 positiveInfinity,
                                    TFloat16 negativeInfinity, TFloat16 finiteMaximum,
                                    TFloat16 finiteMinimum) {
        EXPECT_EQ(static_cast<TInteger>(nan), TInteger{0});
        const TInteger expectedPositive = [] {
            if constexpr (sizeof(TInteger) < sizeof(SharpRuntime::intcs)) {
                return static_cast<TInteger>(std::numeric_limits<SharpRuntime::intcs>::max());
            }
            return std::numeric_limits<TInteger>::max();
        }();
        EXPECT_EQ(static_cast<TInteger>(positiveInfinity), expectedPositive);

        const TInteger expectedNegative = [] {
            if constexpr (sizeof(TInteger) < sizeof(SharpRuntime::intcs)) {
                return static_cast<TInteger>(std::numeric_limits<SharpRuntime::intcs>::lowest());
            } else if constexpr (std::is_unsigned_v<TInteger>) {
                return TInteger{0};
            } else {
                return std::numeric_limits<TInteger>::lowest();
            }
        }();
        EXPECT_EQ(static_cast<TInteger>(negativeInfinity), expectedNegative);
        EXPECT_EQ(static_cast<TInteger>(finiteMaximum), expectedPositive);
        EXPECT_EQ(static_cast<TInteger>(finiteMinimum), expectedNegative);
    }

    template<typename TFloat16>
    void expectEveryIntegralEdge(TFloat16 nan, TFloat16 positiveInfinity,
                                 TFloat16 negativeInfinity, TFloat16 finiteMaximum,
                                 TFloat16 finiteMinimum) {
        expectDefinedIntegralEdges<SharpRuntime::charcs>(
            nan, positiveInfinity, negativeInfinity, finiteMaximum, finiteMinimum);
        expectDefinedIntegralEdges<SharpRuntime::bytecs>(
            nan, positiveInfinity, negativeInfinity, finiteMaximum, finiteMinimum);
        expectDefinedIntegralEdges<SharpRuntime::sbytecs>(
            nan, positiveInfinity, negativeInfinity, finiteMaximum, finiteMinimum);
        expectDefinedIntegralEdges<SharpRuntime::shortcs>(
            nan, positiveInfinity, negativeInfinity, finiteMaximum, finiteMinimum);
        expectDefinedIntegralEdges<SharpRuntime::ushortcs>(
            nan, positiveInfinity, negativeInfinity, finiteMaximum, finiteMinimum);
        expectDefinedIntegralEdges<SharpRuntime::intcs>(
            nan, positiveInfinity, negativeInfinity, finiteMaximum, finiteMinimum);
        expectDefinedIntegralEdges<SharpRuntime::uintcs>(
            nan, positiveInfinity, negativeInfinity, finiteMaximum, finiteMinimum);
        expectDefinedIntegralEdges<SharpRuntime::longcs>(
            nan, positiveInfinity, negativeInfinity, finiteMaximum, finiteMinimum);
        expectDefinedIntegralEdges<SharpRuntime::ulongcs>(
            nan, positiveInfinity, negativeInfinity, finiteMaximum, finiteMinimum);
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
    const BFloat16 a = BFloat16::FromBits(0x3F81u);
    const BFloat16 b = BFloat16::FromBits(0x3F82u);
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

// ===========================================================================
// #2382 (SR-AUD-176 unit A) — the fifteen members `System::Half` had and this
// type did not, plus `IsZero`.
//
// The decomposition (docs/BFloat16SurfaceDecomposition.md) is "bring BFloat16
// to Half's decided LINE", and these cases are what stop that being read as
// "copy Half's BODIES" — .NET's BFloat16 delegates its identity trio to
// `float` where this port's Half implements its own bit-level versions, and
// two of the three then disagree observably.
// ===========================================================================

TEST(BFloat16SurfaceTests, Fix2382_ClassificationCoversTheWholeDomain) {
    const BFloat16 zero      = BFloat16::FromBits(uint16_t(0x0000));
    const BFloat16 negZero    = BFloat16::FromBits(uint16_t(0x8000));
    const BFloat16 subnormal  = BFloat16::FromBits(uint16_t(0x0001));   // the largest-magnitude one is 0x007F
    const BFloat16 maxSubnorm = BFloat16::FromBits(uint16_t(0x007F));
    const BFloat16 minNormal  = BFloat16::FromBits(uint16_t(0x0080));
    const BFloat16 one        = BFloat16::FromBits(uint16_t(0x3F80));
    const BFloat16 inf        = BFloat16::FromBits(uint16_t(0x7F80));
    const BFloat16 negInf     = BFloat16::FromBits(uint16_t(0xFF80));
    const BFloat16 nan        = BFloat16::FromBits(uint16_t(0x7FC0));
    const BFloat16 negNan     = BFloat16::FromBits(uint16_t(0xFFC0));

    // IsFinite
    for (const BFloat16& v : {zero, negZero, subnormal, maxSubnorm, minNormal, one})
        EXPECT_TRUE(BFloat16::IsFinite(v));
    for (const BFloat16& v : {inf, negInf, nan, negNan})
        EXPECT_FALSE(BFloat16::IsFinite(v));

    // IsZero -- the one member Half does not have, because .NET's Half does not publish it.
    EXPECT_TRUE(BFloat16::IsZero(zero));
    EXPECT_TRUE(BFloat16::IsZero(negZero));
    EXPECT_FALSE(BFloat16::IsZero(subnormal));

    // IsNormal / IsSubnormal partition the finite nonzero values, and the boundary is
    // 0x0080 rather than Half's 0x0400 -- eight exponent bits, not five.
    EXPECT_FALSE(BFloat16::IsNormal(zero));
    EXPECT_FALSE(BFloat16::IsNormal(subnormal));
    EXPECT_FALSE(BFloat16::IsNormal(maxSubnorm));
    EXPECT_TRUE(BFloat16::IsNormal(minNormal));
    EXPECT_TRUE(BFloat16::IsNormal(one));
    EXPECT_FALSE(BFloat16::IsNormal(inf));
    EXPECT_FALSE(BFloat16::IsNormal(nan));

    EXPECT_FALSE(BFloat16::IsSubnormal(zero));
    EXPECT_TRUE(BFloat16::IsSubnormal(subnormal));
    EXPECT_TRUE(BFloat16::IsSubnormal(maxSubnorm));
    EXPECT_FALSE(BFloat16::IsSubnormal(minNormal));
    EXPECT_FALSE(BFloat16::IsSubnormal(inf));

    // ...and every value is in exactly one of the four INumberBase classes.
    for (uint16_t raw = 0;; ++raw) {
        const BFloat16 v = BFloat16::FromBits(raw);
        const int classes = int(BFloat16::IsZero(v)) + int(BFloat16::IsSubnormal(v))
                          + int(BFloat16::IsNormal(v)) + int(!BFloat16::IsFinite(v));
        ASSERT_EQ(classes, 1) << "bit pattern 0x" << std::hex << raw << " is in " << classes;
        if (raw == 0xFFFF) break;
    }
}

TEST(BFloat16SurfaceTests, Fix2382_IsNegativeIsASignBitTestNotAComparison) {
    // BFloat16.cs:181-185 is `(short)(value._value) < 0`. So negative zero and a negative
    // NaN are both negative, which `v < BFloat16()` would report as false for both.
    EXPECT_TRUE(BFloat16::IsNegative(BFloat16::FromBits(uint16_t(0x8000))));   // -0
    EXPECT_FALSE(BFloat16::IsNegative(BFloat16::FromBits(uint16_t(0x0000))));  // +0
    EXPECT_TRUE(BFloat16::IsNegative(BFloat16::FromBits(uint16_t(0xFFC0))));   // -NaN
    EXPECT_FALSE(BFloat16::IsNegative(BFloat16::FromBits(uint16_t(0x7FC0))));  // +NaN
    EXPECT_TRUE(BFloat16::IsNegative(BFloat16::FromBits(uint16_t(0xFF80))));   // -Infinity
    EXPECT_TRUE(BFloat16::IsNegative(BFloat16(-1.0f)));
    EXPECT_FALSE(BFloat16::IsNegative(BFloat16(1.0f)));
}

TEST(BFloat16SurfaceTests, Fix2382_TheIdentityTrioIsFloatsNotHalfs) {
    const BFloat16 nan  = BFloat16::FromBits(uint16_t(0x7FC0));
    const BFloat16 nan2 = BFloat16::FromBits(uint16_t(0xFFC0));
    const BFloat16 zero = BFloat16::FromBits(uint16_t(0x0000));
    const BFloat16 negZero = BFloat16::FromBits(uint16_t(0x8000));

    // Equals is NOT operator== : NaN equals NaN, and the two zeros are equal.
    EXPECT_TRUE(nan.Equals(nan));
    EXPECT_TRUE(nan.Equals(nan2));
    EXPECT_FALSE(nan == nan);
    EXPECT_TRUE(zero.Equals(negZero));
    EXPECT_TRUE(zero == negZero);
    EXPECT_FALSE(zero.Equals(BFloat16(1.0f)));

    // CompareTo: NaN sorts before every number and equals itself (Single::CompareTo).
    EXPECT_EQ(nan.CompareTo(nan2), 0);
    EXPECT_LT(nan.CompareTo(zero), 0);
    EXPECT_GT(zero.CompareTo(nan), 0);
    EXPECT_LT(BFloat16(1.0f).CompareTo(BFloat16(2.0f)), 0);
    EXPECT_GT(BFloat16(2.0f).CompareTo(BFloat16(1.0f)), 0);
    EXPECT_EQ(BFloat16(2.0f).CompareTo(BFloat16(2.0f)), 0);
    EXPECT_EQ(zero.CompareTo(negZero), 0);

    // THE assertion that discriminates "derived from BFloat16.cs" from "copied from
    // Half.hpp". .NET's BFloat16 hash is `((float)this).GetHashCode()` -- float's, over the
    // WIDENED value -- while this port's Half::GetHashCode masks to 16 bits and can only
    // ever return a value below 0x10000. Copying Half here would have compiled, satisfied
    // the hash contract, and been wrong.
    EXPECT_EQ(BFloat16(1.0f).GetHashCode(), System::Single::GetHashCode(1.0f));
    EXPECT_GT(BFloat16(1.0f).GetHashCode(), 0x10000);

    // The contract itself: values that Equals() calls equal must hash equally.
    EXPECT_EQ(nan.GetHashCode(), nan2.GetHashCode());
    EXPECT_EQ(zero.GetHashCode(), negZero.GetHashCode());
}

TEST(BFloat16SurfaceTests, Fix2382_ParseAndTryParseRoundTrip) {
    EXPECT_FLOAT_EQ(static_cast<float>(BFloat16::Parse("1")), 1.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(BFloat16::Parse("-2.5")), -2.5f);
    EXPECT_FLOAT_EQ(static_cast<float>(BFloat16::Parse("0")), 0.0f);
    EXPECT_THROW((void)BFloat16::Parse("not a number"), System::FormatException);
    // The provider overload accepts and ignores its argument, as the class note says.
    EXPECT_FLOAT_EQ(static_cast<float>(BFloat16::Parse("3", nullptr)), 3.0f);

    BFloat16 out = BFloat16(1.0f);
    EXPECT_TRUE(BFloat16::TryParse("4", out));
    EXPECT_FLOAT_EQ(static_cast<float>(out), 4.0f);
    EXPECT_TRUE(BFloat16::TryParse("5", nullptr, out));
    EXPECT_FLOAT_EQ(static_cast<float>(out), 5.0f);

    // On failure the out parameter is ZERO, not left holding its previous value.
    out = BFloat16(9.0f);
    EXPECT_FALSE(BFloat16::TryParse("zzz", out));
    EXPECT_TRUE(BFloat16::IsZero(out));

    // Parsing goes through the SAME narrowing as the float constructor, so a value with no
    // exact BFloat16 is rounded rather than rejected.
    EXPECT_EQ(BFloat16::Parse("0.1").getBitsProperty(), BFloat16(0.1f).getBitsProperty());
}

TEST(BFloat16SurfaceTests, Fix2382_FormattingAndTryFormat) {
    EXPECT_EQ(BFloat16(1.0f).ToString(""), BFloat16(1.0f).ToString());
    EXPECT_EQ(BFloat16(1.5f).ToString("F2"), "1.50");
    EXPECT_EQ(BFloat16(1.0f).ToString(nullptr), BFloat16(1.0f).ToString());
    EXPECT_EQ(BFloat16(1.5f).ToString("F2", nullptr), "1.50");

    // The three non-finite values get .NET's names at EVERY format, including the default
    // one. That last part is a REPAIR #2382 made on the way past, not a property the type
    // already had: `ToString()` was a bare `std::to_chars` -- a second formatter beside
    // `System::Single::ToString` -- and the two disagreed on 256 of the 65,536 bit patterns,
    // C's lowercase "inf"/"nan" against .NET's "Infinity"/"NaN". The row below the loop is
    // the one that would have failed.
    for (const char* fmt : {"F2", "E3", "G", "R", ""}) {
        SCOPED_TRACE(fmt);
        EXPECT_EQ(BFloat16::FromBits(uint16_t(0x7FC0)).ToString(fmt), "NaN");
        EXPECT_EQ(BFloat16::FromBits(uint16_t(0x7F80)).ToString(fmt), "Infinity");
        EXPECT_EQ(BFloat16::FromBits(uint16_t(0xFF80)).ToString(fmt), "-Infinity");
    }
    EXPECT_EQ(BFloat16::FromBits(uint16_t(0x7FC0)).ToString(), "NaN");
    EXPECT_EQ(BFloat16::FromBits(uint16_t(0x7F80)).ToString(), "Infinity");
    EXPECT_EQ(BFloat16::FromBits(uint16_t(0xFF80)).ToString(), "-Infinity");

    // ...and there is now ONE formatter, which is what stops the two drifting apart again:
    // every bit pattern formats identically through the default and the empty format.
    for (uint32_t r = 0;; ++r) {
        const BFloat16 v = BFloat16::FromBits(static_cast<uint16_t>(r));
        ASSERT_EQ(v.ToString(), v.ToString("")) << "bit pattern 0x" << std::hex << r;
        if (r == 0xFFFF) break;
    }

    std::array<char, 16> buf{};
    SharpRuntime::intcs written = -1;
    System::Span<char> span(buf.data(), static_cast<SharpRuntime::intcs>(buf.size()));
    EXPECT_TRUE(BFloat16(1.5f).TryFormat(span, written, "F2"));
    EXPECT_EQ(written, 4);
    EXPECT_EQ(std::string(buf.data(), static_cast<size_t>(written)), "1.50");

    // Too short: nothing is written and charsWritten is zero, rather than a truncated prefix.
    std::array<char, 2> tiny{'X', 'Y'};
    System::Span<char> tinySpan(tiny.data(), 2);
    written = -1;
    EXPECT_FALSE(BFloat16(1.5f).TryFormat(tinySpan, written, "F2"));
    EXPECT_EQ(written, 0);
    EXPECT_EQ(tiny[0], 'X');
    EXPECT_EQ(tiny[1], 'Y');
}

template <typename T> concept HasAbs          = requires(T v) { T::Abs(v); };
template <typename T> concept HasBitIncrement = requires(T v) { T::BitIncrement(v); };
template <typename T> concept HasCopySign     = requires(T v) { T::CopySign(v, v); };
template <typename T> concept HasSqrt         = requires(T v) { T::Sqrt(v); };
template <typename T> concept HasIsFinite     = requires(T v) { T::IsFinite(v); };
template <typename T> concept HasGetHashCode  = requires(const T& v) { v.GetHashCode(); };
template <typename T> concept HasMaxNative    = requires(T v) { T::MaxNative(v, v); };

TEST(BFloat16SurfaceTests, Fix2384_TheInStepRequirementHeldWhenUnit1Landed) {
    // PARTIALLY INVERTED BY #2384 UNIT 1. Its predecessor asserted that NONE of this surface
    // existed and said, in every message, "it must move System::Half too". That is #2340's
    // in-step rule, and it WORKED AS DESIGNED: adding these three members to BFloat16 alone broke
    // the build, which is exactly the signal it was written to give.
    //
    // The dependent parameter is KEPT -- gcc evaluates a non-dependent `requires` eagerly and
    // errors on a missing name instead of yielding false (#2299, hit again writing the original
    // of this case). It is what lets presence and absence be asserted in the same form.
    static_assert(HasAbs<BFloat16>,          "#2384 unit 1");
    static_assert(HasBitIncrement<BFloat16>, "#2384 unit 1");
    static_assert(HasCopySign<BFloat16>,     "#2384 unit 1");

    // AND THE SAME MEMBERS EXIST ON Half. This is the assertion that enforces #2340's rule now
    // that the surface is arriving: a unit that moved one type and not the other fails HERE
    // rather than being discovered later as an inconsistency between the port's two 16-bit
    // floats -- which #2340 measured as worse than either policy applied consistently.
    static_assert(HasAbs<System::Half>,          "#2384: units move BOTH 16-bit float types");
    static_assert(HasBitIncrement<System::Half>, "#2384: units move BOTH 16-bit float types");
    static_assert(HasCopySign<System::Half>,     "#2384: units move BOTH 16-bit float types");

    // UNIT 2b HAS NOW LANDED, on both types in the same change -- which is the signal the two
    // assertions below used to withhold. #2340's rule held again: Sqrt could not arrive on one
    // type alone without breaking this.
    static_assert(HasSqrt<BFloat16>,     "#2384 unit 2b");
    static_assert(HasSqrt<System::Half>, "#2384 unit 2b -- and it moved BOTH types");

    // ...and #2382's members still exist, so no half of this pin is satisfied by the type simply
    // failing to compile.
    static_assert(HasIsFinite<BFloat16>);
    static_assert(HasGetHashCode<BFloat16>);
    SUCCEED();
}

TEST(BFloat16SurfaceTests, Fix2382_LayoutIsStillTwoBytes) {
    // Sixteen members, all static or const, so the representation is untouched.
    EXPECT_EQ(sizeof(BFloat16), 2u);
    EXPECT_EQ(alignof(BFloat16), 2u);
    static_assert(std::is_trivially_copyable_v<BFloat16>);
}

// =================================================================================================
// #2384 unit 1 -- the sign/magnitude family, on BOTH 16-bit float types.
//
// #2340 established that this surface must move for System::Half and System::Numerics::BFloat16
// TOGETHER or not at all: adding it to one leaves the port's two 16-bit floats inconsistent with
// each other, which is worse than either policy applied consistently. So every case below asserts
// the same property on both types, side by side, and a member added to only one fails to compile.
// =================================================================================================

TEST(Fix2384Unit1, AbsAndCopySignAreBitOperationsNotFloatRoundTrips) {
    // .NET's Abs is `value._value & ~SignMask` (Half.cs:1756, BFloat16.cs:1396) and its CopySign
    // says in a comment of its own that it "is required to work for all inputs, including NaN, so
    // we operate on the raw bits". A blanket "forward to float" would be wrong for both, and the
    // NaN rows are what discriminate: this port canonicalises a NaN through FromSingle, so a
    // round-trip implementation would lose a non-canonical payload that masking keeps.
    using System::Half;
    using System::Numerics::BFloat16;

    // A NaN with a payload bit that a float round-trip in this port would canonicalise.
    const Half halfNaN = Half::FromBits(static_cast<uint16_t>(0x7E01u));
    // A SIGNALLING NaN, deliberately. The first cut used 0x7FC1, which is already QUIET, so a
    // float round-trip returned it unchanged and the mutation that replaces masking with
    // std::fabs went UNCAUGHT. Measured over all 65,536 patterns, the two forms differ on
    // exactly 126 -- every one a signalling NaN, which fromFloat quiets by OR-ing in 0x0040.
    const BFloat16 bfNaN = BFloat16::FromBits(static_cast<uint16_t>(0x7F81u));
    ASSERT_TRUE(Half::IsNaN(halfNaN));
    ASSERT_TRUE(BFloat16::IsNaN(bfNaN));

    EXPECT_EQ(Half::Abs(halfNaN).bits, 0x7E01u) << "#2384: Abs masks, it does not round-trip";
    EXPECT_EQ(Half::Abs(Half::FromBits(static_cast<uint16_t>(0xFE01u))).bits, 0x7E01u);
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::Abs(bfNaN)), 0x7F81u)
        << "#2384: Abs MASKS, so a signalling NaN stays signalling; a float round-trip would "
           "quiet it to 0x7FC1";
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::Abs(BFloat16::FromBits(static_cast<uint16_t>(0xFF81u)))),
              0x7F81u) << "and the sign is cleared without disturbing the payload";

    // Ordinary magnitudes, both signs, and the signed zeros -- Abs(-0.0) is +0.0 by masking.
    EXPECT_EQ(Half::Abs(Half::FromSingle(-2.5f)).bits, Half::FromSingle(2.5f).bits);
    EXPECT_EQ(Half::Abs(Half::FromBits(static_cast<uint16_t>(0x8000u))).bits, 0x0000u);
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::Abs(BFloat16(-2.5f))),
              std::bit_cast<uint16_t>(BFloat16(2.5f)));

    // CopySign takes the magnitude of the first and the sign of the second, including onto NaN.
    EXPECT_EQ(Half::CopySign(Half::FromSingle(2.5f), Half::FromSingle(-1.0f)).bits,
              Half::FromSingle(-2.5f).bits);
    EXPECT_EQ(Half::CopySign(Half::FromSingle(-2.5f), Half::FromSingle(1.0f)).bits,
              Half::FromSingle(2.5f).bits);
    EXPECT_EQ(Half::CopySign(halfNaN, Half::FromSingle(-1.0f)).bits, 0xFE01u)
        << "#2384: the payload survives, which is why this is bitwise";
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::CopySign(BFloat16(2.5f), BFloat16(-1.0f))), std::bit_cast<uint16_t>(BFloat16(-2.5f)));

    // Signed zero: CopySign(+0, -x) is -0, which no comparison can see -- so the BITS are asserted.
    EXPECT_EQ(Half::CopySign(Half::FromBits(static_cast<uint16_t>(0x0000u)),
                             Half::FromSingle(-1.0f)).bits, 0x8000u);
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::CopySign(BFloat16::FromBits(uint16_t(0x0000u)), BFloat16(-1.0f))), 0x8000u);
}

TEST(Fix2384Unit1, BitIncrementAndBitDecrementTranscribeTheirEdges) {
    // The three edges that are NOT arithmetic, transcribed from Half.cs:1445-1508 and
    // BFloat16.cs:1101-1164, and asserted on both types because they are the rows a plausible
    // "add one to the bits" implementation gets wrong.
    using System::Half;
    using System::Numerics::BFloat16;

    // -0.0 -> Epsilon, NOT +0.0.
    EXPECT_EQ(Half::BitIncrement(Half::FromBits(static_cast<uint16_t>(0x8000u))).bits, Half::Epsilon.bits);
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::BitIncrement(BFloat16::FromBits(uint16_t(0x8000u)))), std::bit_cast<uint16_t>(BFloat16::Epsilon()));

    // +0.0 -> -Epsilon.
    EXPECT_EQ(Half::BitDecrement(Half::FromBits(static_cast<uint16_t>(0x0000u))).bits,
              static_cast<uint16_t>(Half::Epsilon.bits | 0x8000u));
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::BitDecrement(BFloat16::FromBits(uint16_t(0x0000u)))),
              static_cast<uint16_t>(std::bit_cast<uint16_t>(BFloat16::Epsilon()) | 0x8000u));

    // -Infinity increments to MinValue; +Infinity decrements to MaxValue.
    EXPECT_EQ(Half::BitIncrement(Half::NegativeInfinity).bits, Half::MinValue.bits);
    EXPECT_EQ(Half::BitDecrement(Half::PositiveInfinity).bits, Half::MaxValue.bits);
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::BitIncrement(BFloat16::NegativeInfinity())), std::bit_cast<uint16_t>(BFloat16::MinValue()));
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::BitDecrement(BFloat16::PositiveInfinity())), std::bit_cast<uint16_t>(BFloat16::MaxValue()));

    // The infinities in the OTHER direction, and NaN, return themselves.
    EXPECT_EQ(Half::BitIncrement(Half::PositiveInfinity).bits, Half::PositiveInfinity.bits);
    EXPECT_EQ(Half::BitDecrement(Half::NegativeInfinity).bits, Half::NegativeInfinity.bits);
    EXPECT_TRUE(Half::IsNaN(Half::BitIncrement(Half::NaN)));
    EXPECT_TRUE(Half::IsNaN(Half::BitDecrement(Half::NaN)));
    EXPECT_TRUE(BFloat16::IsNaN(BFloat16::BitIncrement(BFloat16::NaN())));

    // And the ordinary case is a real step: increment then decrement returns the original, and
    // the step is the SMALLEST one -- nothing lies strictly between.
    const Half one = Half::FromSingle(1.0f);
    const Half next = Half::BitIncrement(one);
    EXPECT_NE(next.bits, one.bits);
    EXPECT_EQ(Half::BitDecrement(next).bits, one.bits);
    EXPECT_EQ(static_cast<uint16_t>(next.bits - one.bits), 1u);

    // Negative values step the OTHER way through the bit pattern, which is the branch a naive
    // implementation drops.
    const Half minusOne = Half::FromSingle(-1.0f);
    EXPECT_LT((Half::BitDecrement(minusOne)).ToSingle(), -1.0f);
    EXPECT_GT((Half::BitIncrement(minusOne)).ToSingle(), -1.0f);
}

TEST(Fix2384Unit1, ClampMaxMinAndMagnitudeGoThroughFloatAsDotNetDoes) {
    using System::Half;
    using System::Numerics::BFloat16;

    EXPECT_EQ((Half::Clamp(Half::FromSingle(5.0f), Half::FromSingle(1.0f),
                                         Half::FromSingle(3.0f))).ToSingle(), 3.0f);
    EXPECT_EQ((Half::Clamp(Half::FromSingle(-5.0f), Half::FromSingle(-3.0f),
                                         Half::FromSingle(3.0f))).ToSingle(), -3.0f);
    EXPECT_EQ((Half::Clamp(Half::FromSingle(2.0f), Half::FromSingle(1.0f),
                                         Half::FromSingle(3.0f))).ToSingle(), 2.0f);
    EXPECT_EQ(static_cast<float>(BFloat16::Clamp(BFloat16(5.0f), BFloat16(1.0f),
                                                BFloat16(3.0f))), 3.0f);

    EXPECT_EQ((Half::Max(Half::FromSingle(1.0f), Half::FromSingle(2.0f))).ToSingle(), 2.0f);
    EXPECT_EQ((Half::Min(Half::FromSingle(1.0f), Half::FromSingle(2.0f))).ToSingle(), 1.0f);
    EXPECT_EQ(static_cast<float>(BFloat16::Max(BFloat16(1.0f), BFloat16(2.0f))), 2.0f);
    EXPECT_EQ(static_cast<float>(BFloat16::Min(BFloat16(1.0f), BFloat16(2.0f))), 1.0f);

    // MAGNITUDE, not value -- the row that separates these from Max/Min, and the one a
    // copy-paste of Max would pass every other assertion while failing.
    EXPECT_EQ((Half::MaxMagnitude(Half::FromSingle(-3.0f),
                                                Half::FromSingle(2.0f))).ToSingle(), -3.0f);
    EXPECT_EQ((Half::MinMagnitude(Half::FromSingle(-3.0f),
                                                Half::FromSingle(2.0f))).ToSingle(), 2.0f);
    EXPECT_EQ(static_cast<float>(BFloat16::MaxMagnitude(BFloat16(-3.0f), BFloat16(2.0f))),
              -3.0f);
    EXPECT_EQ(static_cast<float>(BFloat16::MinMagnitude(BFloat16(-3.0f), BFloat16(2.0f))),
              2.0f);
}

// =================================================================================================
// #2384 unit 2a -- rounding, Sign, and the IEEE 754:2019 *Number family, on BOTH types.
// =================================================================================================

TEST(Fix2384Unit2a, TheNumberFamilyDoesNotPropagateNaNWhereMaxAndMinDo) {
    // THE ROW THAT SEPARATES THEM, and the one a forward to Max/Min passes every other assertion
    // while failing. .NET's own comment: `maximumNumber` "does not propagate NaN inputs back to
    // the caller and otherwise returns the larger of the inputs".
    using System::Half;
    using System::Numerics::BFloat16;

    const Half hNaN = Half::NaN;
    const Half hTwo = Half::FromSingle(2.0f);
    EXPECT_TRUE(Half::IsNaN(Half::Max(hNaN, hTwo))) << "Max PROPAGATES NaN";
    EXPECT_TRUE(Half::IsNaN(Half::Min(hNaN, hTwo)));
    EXPECT_EQ(Half::MaxNumber(hNaN, hTwo).ToSingle(), 2.0f) << "MaxNumber IGNORES it";
    EXPECT_EQ(Half::MaxNumber(hTwo, hNaN).ToSingle(), 2.0f) << "from either side";
    EXPECT_EQ(Half::MinNumber(hNaN, hTwo).ToSingle(), 2.0f);
    EXPECT_EQ(Half::MinNumber(hTwo, hNaN).ToSingle(), 2.0f);

    const BFloat16 bNaN = BFloat16::NaN();
    const BFloat16 bTwo(2.0f);
    EXPECT_TRUE(BFloat16::IsNaN(BFloat16::Max(bNaN, bTwo)));
    EXPECT_EQ(static_cast<float>(BFloat16::MaxNumber(bNaN, bTwo)), 2.0f);
    EXPECT_EQ(static_cast<float>(BFloat16::MinNumber(bTwo, bNaN)), 2.0f);

    // Ordinary ordering is unchanged, so the NaN rows above are not passing because the family
    // is broken generally.
    EXPECT_EQ(Half::MaxNumber(Half::FromSingle(1.0f), hTwo).ToSingle(), 2.0f);
    EXPECT_EQ(Half::MinNumber(Half::FromSingle(1.0f), hTwo).ToSingle(), 1.0f);
}

TEST(Fix2384Unit2a, TheNumberFamilyTreatsPositiveZeroAsLargerThanNegativeZero) {
    // The second IEEE rule, and NO COMPARISON CAN SEE IT -- +0.0 == -0.0 is true -- so the BITS
    // are asserted. A naive `(x > y) ? x : y` returns the wrong zero here and passes everything
    // else in this file.
    using System::Half;
    using System::Numerics::BFloat16;
    const Half posZero = Half::FromBits(static_cast<uint16_t>(0x0000u));
    const Half negZero = Half::FromBits(static_cast<uint16_t>(0x8000u));
    ASSERT_EQ(posZero.ToSingle(), negZero.ToSingle()) << "they compare equal, which is the point";

    EXPECT_EQ(Half::MaxNumber(posZero, negZero).bits, 0x0000u);
    EXPECT_EQ(Half::MaxNumber(negZero, posZero).bits, 0x0000u);
    EXPECT_EQ(Half::MinNumber(posZero, negZero).bits, 0x8000u);
    EXPECT_EQ(Half::MinNumber(negZero, posZero).bits, 0x8000u);

    const BFloat16 bPos = BFloat16::FromBits(static_cast<uint16_t>(0x0000u));
    const BFloat16 bNeg = BFloat16::FromBits(static_cast<uint16_t>(0x8000u));
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::MaxNumber(bPos, bNeg)), 0x0000u);
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::MinNumber(bNeg, bPos)), 0x8000u);

    // The magnitude variants have the same tie rule, in the opposite direction for Min.
    EXPECT_EQ(Half::MaxMagnitudeNumber(negZero, posZero).bits, 0x0000u);
    EXPECT_EQ(Half::MinMagnitudeNumber(negZero, posZero).bits, 0x8000u);
    // ...and they select by MAGNITUDE, which is what separates them from MaxNumber/MinNumber.
    EXPECT_EQ(Half::MaxMagnitudeNumber(Half::FromSingle(-3.0f),
                                       Half::FromSingle(2.0f)).ToSingle(), -3.0f);
    EXPECT_EQ(Half::MinMagnitudeNumber(Half::FromSingle(-3.0f),
                                       Half::FromSingle(2.0f)).ToSingle(), 2.0f);
    EXPECT_EQ(static_cast<float>(BFloat16::MaxMagnitudeNumber(BFloat16(-3.0f), BFloat16(2.0f))),
              -3.0f);
}

TEST(Fix2384Unit2a, SignThrowsOnNaNAndReturnsZeroForBothSignedZeros) {
    // .NET throws ArithmeticException(SR.Arithmetic_NaN) rather than returning a sentinel, and it
    // tests IsZero BEFORE IsNegative -- so Sign(-0.0) is 0, not -1. Both are transcribed.
    using System::Half;
    using System::Numerics::BFloat16;

    EXPECT_THROW((void)Half::Sign(Half::NaN), System::ArithmeticException);
    EXPECT_THROW((void)BFloat16::Sign(BFloat16::NaN()), System::ArithmeticException);

    EXPECT_EQ(Half::Sign(Half::FromBits(static_cast<uint16_t>(0x0000u))), 0);
    EXPECT_EQ(Half::Sign(Half::FromBits(static_cast<uint16_t>(0x8000u))), 0) << "-0.0 is ZERO, not negative";
    EXPECT_EQ(BFloat16::Sign(BFloat16::FromBits(static_cast<uint16_t>(0x8000u))), 0);

    EXPECT_EQ(Half::Sign(Half::FromSingle(2.5f)), 1);
    EXPECT_EQ(Half::Sign(Half::FromSingle(-2.5f)), -1);
    EXPECT_EQ(Half::Sign(Half::PositiveInfinity), 1);
    EXPECT_EQ(Half::Sign(Half::NegativeInfinity), -1);
    EXPECT_EQ(BFloat16::Sign(BFloat16(2.5f)), 1);
    EXPECT_EQ(BFloat16::Sign(BFloat16(-2.5f)), -1);
}

TEST(Fix2384Unit2a, TheRoundingFamilyGoesThroughFloatAndRoundsTiesToEven) {
    using System::Half;
    using System::Numerics::BFloat16;
    EXPECT_EQ(Half::Ceiling(Half::FromSingle(1.25f)).ToSingle(), 2.0f);
    EXPECT_EQ(Half::Floor(Half::FromSingle(1.75f)).ToSingle(), 1.0f);
    EXPECT_EQ(Half::Truncate(Half::FromSingle(-1.75f)).ToSingle(), -1.0f);
    EXPECT_EQ(Half::Ceiling(Half::FromSingle(-1.25f)).ToSingle(), -1.0f);

    // TIES TO EVEN, not away from zero -- the row that separates .NET's MathF.Round from a naive
    // std::round, and the reason this forwards to MathF rather than to <cmath> directly.
    EXPECT_EQ(Half::Round(Half::FromSingle(0.5f)).ToSingle(), 0.0f);
    EXPECT_EQ(Half::Round(Half::FromSingle(1.5f)).ToSingle(), 2.0f);
    EXPECT_EQ(Half::Round(Half::FromSingle(2.5f)).ToSingle(), 2.0f);
    EXPECT_EQ(static_cast<float>(BFloat16::Round(BFloat16(2.5f))), 2.0f);
    EXPECT_EQ(static_cast<float>(BFloat16::Ceiling(BFloat16(1.25f))), 2.0f);
    EXPECT_EQ(static_cast<float>(BFloat16::Truncate(BFloat16(-1.75f))), -1.0f);

    // The digits overload exists on both.
    EXPECT_EQ(Half::Round(Half::FromSingle(1.0f), 2).ToSingle(), 1.0f);
    EXPECT_EQ(static_cast<float>(BFloat16::Round(BFloat16(1.0f), 2)), 1.0f);
}

TEST(Fix2384Unit2a, Decl2384_TheFourHalfOnlyMembersAreAbsentFromBFloat16Deliberately) {
    // #2340's in-step rule means EACH TYPE GETS WHAT .NET GIVES IT, not that the two surfaces are
    // identical. Measured by diffing the two ref surfaces: MaxNative, MinNative, ClampNative and
    // MultiplyAddEstimate are declared on Half ONLY.
    //
    // Pinned so their absence on BFloat16 is a transcription rather than an oversight, and so a
    // future unit that "completes" the symmetry has to justify inventing them.
    static_assert(HasMaxNative<System::Half>, "#2384: .NET declares MaxNative on Half");
    static_assert(!HasMaxNative<System::Numerics::BFloat16>,
                  "#2384: .NET does NOT declare MaxNative on BFloat16 -- adding it would be "
                  "invention, not parity");

    using System::Half;
    EXPECT_EQ(Half::MaxNative(Half::FromSingle(1.0f), Half::FromSingle(2.0f)).ToSingle(), 2.0f);
    EXPECT_EQ(Half::MinNative(Half::FromSingle(1.0f), Half::FromSingle(2.0f)).ToSingle(), 1.0f);
}

// =================================================================================================
// #2384 unit 2b -- the transcendental, root, power and angular families, on BOTH types.
//
// Every one is a float round-trip, which is .NET's own shape (verified member by member against
// Half.cs and BFloat16.cs), so there is no bit-level body to get wrong here. What IS worth
// asserting is that each name reaches the RIGHT float function -- a forwarding table is exactly
// the kind of code where Sin ends up calling Cos and every "does it compile" test still passes.
// =================================================================================================

namespace {
    /// Asserts a Half unary member agrees with the float function it forwards to, at 16-bit
    /// precision. Comparing against FromSingle(expected) rather than a literal is what makes this
    /// a test of the FORWARDING rather than of MathF's own accuracy.
    void expectHalfUnary(System::Half (*member)(System::Half), float (*expected)(float), float in,
                         const char* what) {
        const System::Half got = member(System::Half::FromSingle(in));
        const System::Half want = System::Half::FromSingle(expected(in));
        EXPECT_EQ(got.bits, want.bits) << what << " at " << in;
    }
}

TEST(Fix2384Unit2b, EveryUnaryMemberReachesItsOwnFloatFunction) {
    using System::Half;
    using MF = System::MathF;
    using S  = System::Single;

    // 0.5 is chosen so that every one of these is defined and none collapses to a shared value --
    // Sin(0)==Tan(0)==0 would let a mis-wired table pass.
    expectHalfUnary(&Half::Acos,  &MF::Acos,  0.5f, "Acos");
    expectHalfUnary(&Half::Asin,  &MF::Asin,  0.5f, "Asin");
    expectHalfUnary(&Half::Atan,  &MF::Atan,  0.5f, "Atan");
    expectHalfUnary(&Half::Cos,   &MF::Cos,   0.5f, "Cos");
    expectHalfUnary(&Half::Sin,   &MF::Sin,   0.5f, "Sin");
    expectHalfUnary(&Half::Tan,   &MF::Tan,   0.5f, "Tan");
    expectHalfUnary(&Half::Cosh,  &MF::Cosh,  0.5f, "Cosh");
    expectHalfUnary(&Half::Sinh,  &MF::Sinh,  0.5f, "Sinh");
    expectHalfUnary(&Half::Tanh,  &MF::Tanh,  0.5f, "Tanh");
    expectHalfUnary(&Half::Asinh, &MF::Asinh, 0.5f, "Asinh");
    expectHalfUnary(&Half::Atanh, &MF::Atanh, 0.5f, "Atanh");
    expectHalfUnary(&Half::Acosh, &MF::Acosh, 1.5f, "Acosh");   // domain is [1, inf)
    expectHalfUnary(&Half::Exp,   &MF::Exp,   0.5f, "Exp");
    expectHalfUnary(&Half::Log,   &MF::Log,   0.5f, "Log");
    expectHalfUnary(&Half::Log10, &MF::Log10, 0.5f, "Log10");
    expectHalfUnary(&Half::Log2,  &MF::Log2,  0.5f, "Log2");
    expectHalfUnary(&Half::Sqrt,  &MF::Sqrt,  0.5f, "Sqrt");
    expectHalfUnary(&Half::Cbrt,  &MF::Cbrt,  0.5f, "Cbrt");
    expectHalfUnary(&Half::ReciprocalEstimate,     &MF::ReciprocalEstimate,     0.5f, "ReciprocalEstimate");
    expectHalfUnary(&Half::ReciprocalSqrtEstimate, &MF::ReciprocalSqrtEstimate, 0.5f, "ReciprocalSqrtEstimate");

    // The members that forward to System::Single rather than MathF, because MathF has no
    // counterpart for them -- a real split in this port, not a stylistic one.
    expectHalfUnary(&Half::AcosPi, &S::AcosPi, 0.5f, "AcosPi");
    expectHalfUnary(&Half::AsinPi, &S::AsinPi, 0.5f, "AsinPi");
    expectHalfUnary(&Half::AtanPi, &S::AtanPi, 0.5f, "AtanPi");
    expectHalfUnary(&Half::CosPi,  &S::CosPi,  0.5f, "CosPi");
    expectHalfUnary(&Half::SinPi,  &S::SinPi,  0.5f, "SinPi");
    expectHalfUnary(&Half::TanPi,  &S::TanPi,  0.5f, "TanPi");
    expectHalfUnary(&Half::Exp2,   &S::Exp2,   0.5f, "Exp2");
    expectHalfUnary(&Half::Exp10,  &S::Exp10,  0.5f, "Exp10");
    expectHalfUnary(&Half::DegreesToRadians, &S::DegreesToRadians, 90.0f, "DegreesToRadians");
    expectHalfUnary(&Half::RadiansToDegrees, &S::RadiansToDegrees, 1.5f,  "RadiansToDegrees");

    // Cross-check that the table is not wired to one shared function: three of the above must
    // disagree with each other on the same input.
    const Half half05 = Half::FromSingle(0.5f);
    EXPECT_NE(Half::Sin(half05).bits, Half::Cos(half05).bits);
    EXPECT_NE(Half::Sin(half05).bits, Half::Tan(half05).bits);
    EXPECT_NE(Half::Log(half05).bits, Half::Log2(half05).bits);
    EXPECT_NE(Half::Exp2(half05).bits, Half::Exp10(half05).bits);
}

TEST(Fix2384Unit2b, TheMultiArgumentMembersForwardTheirArgumentsInOrder) {
    // Argument ORDER is the thing a forwarding table gets wrong silently, so every one of these
    // uses operands that make the two orders give different answers.
    using System::Half;
    using System::Numerics::BFloat16;
    using MF = System::MathF;
    using S  = System::Single;

    const Half a = Half::FromSingle(2.0f);
    const Half b = Half::FromSingle(3.0f);

    EXPECT_EQ(Half::Pow(a, b).bits, Half::FromSingle(MF::Pow(2.0f, 3.0f)).bits);
    EXPECT_NE(Half::Pow(a, b).bits, Half::Pow(b, a).bits) << "8 vs 9 -- order matters";

    EXPECT_EQ(Half::Atan2(a, b).bits, Half::FromSingle(MF::Atan2(2.0f, 3.0f)).bits);
    EXPECT_NE(Half::Atan2(a, b).bits, Half::Atan2(b, a).bits);
    EXPECT_EQ(Half::Atan2Pi(a, b).bits, Half::FromSingle(S::Atan2Pi(2.0f, 3.0f)).bits);

    EXPECT_EQ(Half::Log(a, b).bits, Half::FromSingle(MF::Log(2.0f, 3.0f)).bits)
        << "the two-argument Log takes (value, newBase) in that order";
    EXPECT_NE(Half::Log(a, b).bits, Half::Log(b, a).bits);

    EXPECT_EQ(Half::Hypot(a, b).bits, Half::FromSingle(S::Hypot(2.0f, 3.0f)).bits);
    EXPECT_EQ(Half::Ieee754Remainder(b, a).bits,
              Half::FromSingle(MF::IEEERemainder(3.0f, 2.0f)).bits)
        << "Ieee754Remainder forwards to MathF::IEEERemainder -- the ONE member whose .NET name "
           "and this port's MathF name differ";

    // The integer-taking pair: the second argument must NOT be converted through float.
    EXPECT_EQ(Half::ScaleB(a, 3).bits, Half::FromSingle(MF::ScaleB(2.0f, 3)).bits);
    EXPECT_EQ(Half::RootN(Half::FromSingle(8.0f), 3).bits,
              Half::FromSingle(S::RootN(8.0f, 3)).bits);

    // Ternary.
    EXPECT_EQ(Half::FusedMultiplyAdd(a, b, Half::FromSingle(1.0f)).bits,
              Half::FromSingle(MF::FusedMultiplyAdd(2.0f, 3.0f, 1.0f)).bits);

    // ...and the same on BFloat16, so the two types really did move together.
    const BFloat16 ba(2.0f), bb(3.0f);
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::Pow(ba, bb)),
              std::bit_cast<uint16_t>(BFloat16(MF::Pow(2.0f, 3.0f))));
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::Atan2(ba, bb)),
              std::bit_cast<uint16_t>(BFloat16(MF::Atan2(2.0f, 3.0f))));
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::Sqrt(BFloat16(4.0f))),
              std::bit_cast<uint16_t>(BFloat16(2.0f)));
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::RootN(BFloat16(8.0f), 3)),
              std::bit_cast<uint16_t>(BFloat16(S::RootN(8.0f, 3))));
}

namespace detail2384 {
    // Dependent parameters throughout -- gcc evaluates a non-dependent `requires` eagerly and
    // hard-errors on a missing name instead of yielding false (#2299).
    template <typename T> concept HasCompound   = requires(T v) { T::Compound(v, v); };
    template <typename T> concept HasExpM1      = requires(T v) { T::ExpM1(v); };
    template <typename T> concept HasExp2M1     = requires(T v) { T::Exp2M1(v); };
    template <typename T> concept HasExp10M1    = requires(T v) { T::Exp10M1(v); };
    template <typename T> concept HasLogP1      = requires(T v) { T::LogP1(v); };
    template <typename T> concept HasLog2P1     = requires(T v) { T::Log2P1(v); };
    template <typename T> concept HasLog10P1    = requires(T v) { T::Log10P1(v); };
    template <typename T> concept HasLerp       = requires(T v) { T::Lerp(v, v, v); };
    template <typename T> concept HasMulAddEst  = requires(T v) { T::MultiplyAddEstimate(v, v, v); };
    template <typename T> concept HasClampNative= requires(T v) { T::ClampNative(v, v, v); };
    template <typename T> concept HasSqrtM      = requires(T v) { T::Sqrt(v); };
}

TEST(Fix2384Unit2b, Decl2384_TenMembersAreAbsentBecauseFloatItselfLacksThem) {
    // MEASURED, not overlooked. .NET declares Compound, ExpM1, Exp2M1, Exp10M1, LogP1, Log2P1,
    // Log10P1, Lerp, MultiplyAddEstimate and ClampNative on its 16-bit floats, and NONE of them
    // has a counterpart in this port's System::MathF or System::Single. Adding them here would
    // mean widening `float`'s OWN surface first -- a different type's public API, and a different
    // question than "should the 16-bit floats carry the MathF-forwarding surface".
    //
    // Pinned so the gap is a recorded boundary rather than an accident, and so whichever ticket
    // widens System::Single trips this and can complete the 16-bit types in the same change.
    using System::Half;
    using System::Numerics::BFloat16;
    namespace D = detail2384;

    static_assert(!D::HasCompound<Half>   && !D::HasCompound<BFloat16>);
    static_assert(!D::HasExpM1<Half>      && !D::HasExpM1<BFloat16>);
    static_assert(!D::HasExp2M1<Half>     && !D::HasExp2M1<BFloat16>);
    static_assert(!D::HasExp10M1<Half>    && !D::HasExp10M1<BFloat16>);
    static_assert(!D::HasLogP1<Half>      && !D::HasLogP1<BFloat16>);
    static_assert(!D::HasLog2P1<Half>     && !D::HasLog2P1<BFloat16>);
    static_assert(!D::HasLog10P1<Half>    && !D::HasLog10P1<BFloat16>);
    static_assert(!D::HasLerp<Half>       && !D::HasLerp<BFloat16>);

    // These two are Half-ONLY in .NET, so their absence here is doubly deliberate: absent because
    // float lacks them, AND absent from BFloat16 because .NET does not declare them there either.
    static_assert(!D::HasMulAddEst<Half>   && !D::HasMulAddEst<BFloat16>);
    static_assert(!D::HasClampNative<Half> && !D::HasClampNative<BFloat16>);

    // ...and unit 2b DID land, so this pin is not satisfied by the whole surface being missing.
    static_assert(D::HasSqrtM<Half> && D::HasSqrtM<BFloat16>,
                  "#2384 unit 2b landed on both types");
    SUCCEED();
}

// =================================================================================================
// #2384 unit 3 -- conversion operators in both directions on BOTH types. Ticket #2395 resolved
// the raw-bits constructor collision by naming the bit-pattern factory before the value-taking
// constructors landed.
// =================================================================================================

TEST(Fix2384Unit3, EveryFromConversionTruncatesTowardZero) {
    using System::Half;
    using System::Numerics::BFloat16;

    const Half h = Half::FromSingle(3.75f);
    EXPECT_EQ(static_cast<SharpRuntime::intcs>(h),   3) << "truncates, does not round";
    EXPECT_EQ(static_cast<SharpRuntime::longcs>(h),  3);
    EXPECT_EQ(static_cast<SharpRuntime::shortcs>(h), 3);
    EXPECT_EQ(static_cast<SharpRuntime::bytecs>(h),  3);
    EXPECT_EQ(static_cast<SharpRuntime::uintcs>(h),  3u);
    EXPECT_EQ(static_cast<SharpRuntime::ulongcs>(h), 3u);
    EXPECT_EQ(static_cast<SharpRuntime::ushortcs>(h),3u);
    EXPECT_EQ(static_cast<SharpRuntime::charcs>(h),  3);

    // Toward zero for NEGATIVES too, which is where "truncate" and "floor" part company.
    const Half neg = Half::FromSingle(-3.75f);
    EXPECT_EQ(static_cast<SharpRuntime::intcs>(neg),   -3) << "toward zero, not -4";
    EXPECT_EQ(static_cast<SharpRuntime::longcs>(neg),  -3);
    EXPECT_EQ(static_cast<SharpRuntime::shortcs>(neg), -3);
    EXPECT_EQ(static_cast<SharpRuntime::sbytecs>(neg), -3);

    const BFloat16 b(3.75f);
    EXPECT_EQ(static_cast<SharpRuntime::intcs>(b),  3);
    EXPECT_EQ(static_cast<SharpRuntime::longcs>(b), 3);
    EXPECT_EQ(static_cast<SharpRuntime::sbytecs>(BFloat16(-3.75f)), -3);

    // The float/double operators that predate this unit still work and are unaffected.
    EXPECT_FLOAT_EQ(static_cast<float>(h), 3.75f);
    EXPECT_DOUBLE_EQ(static_cast<double>(h), 3.75);
}

TEST(FixFinalAuditSanitizer, HalfIntegralConversionsHaveDefinedSpecialValueEdges) {
    // A native float-to-integer cast is undefined for NaN and out-of-range values. Half exposes
    // all of those inputs, so every public integral destination must take the guarded path.
    // Use infinities as the finite edge witnesses too: Half::MaxValue fits int32/int64 and would
    // not exercise their upper guard.
    expectEveryIntegralEdge(System::Half::NaN,
                            System::Half::PositiveInfinity,
                            System::Half::NegativeInfinity,
                            System::Half::PositiveInfinity,
                            System::Half::NegativeInfinity);

    // .NET's current unchecked lowering saturates to int32 before narrowing small targets. These
    // ordinary finite witnesses distinguish that policy from direct destination saturation.
    EXPECT_EQ(static_cast<SharpRuntime::bytecs>(System::Half::FromSingle(-1.0f)), 255);
    EXPECT_EQ(static_cast<SharpRuntime::bytecs>(System::Half::FromSingle(256.0f)), 0);
    EXPECT_EQ(static_cast<SharpRuntime::sbytecs>(System::Half::FromSingle(128.0f)), -128);
    EXPECT_EQ(static_cast<SharpRuntime::shortcs>(System::Half::FromSingle(32768.0f)), -32768);
    EXPECT_EQ(static_cast<SharpRuntime::ushortcs>(System::Half::FromSingle(-1.0f)), 65535);
    EXPECT_EQ(static_cast<SharpRuntime::charcs>(System::Half::FromSingle(-1.0f)), 65535);
    EXPECT_EQ(static_cast<SharpRuntime::intcs>(System::Half::FromBits(0x7D01u)), 0)
        << "a positive signalling NaN also maps to zero";
}

TEST(FixFinalAuditSanitizer, BFloat16IntegralConversionsHaveDefinedFiniteAndSpecialEdges) {
    // BFloat16 has float32's exponent range, so its largest finite magnitudes also exceed every
    // supported integral destination. This distinguishes finite clamping from an infinity-only
    // special case and makes all nine operator bodies visible to -fsanitize=float-cast-overflow.
    expectEveryIntegralEdge(BFloat16::NaN(),
                            BFloat16::PositiveInfinity(),
                            BFloat16::NegativeInfinity(),
                            BFloat16::MaxValue(),
                            BFloat16::MinValue());

    // Exact upper-exclusive powers must clamp, while the adjacent BFloat16 below each boundary
    // must still convert to its own value. Together these catch both a `>=` -> `>` mutation and a
    // guard that clamps one representable value too early.
    EXPECT_EQ(static_cast<SharpRuntime::longcs>(BFloat16::FromBits(0x5F00u)),
              std::numeric_limits<SharpRuntime::longcs>::max());       // 2^63
    EXPECT_EQ(static_cast<SharpRuntime::longcs>(BFloat16::FromBits(0x5EFFu)),
              std::numeric_limits<SharpRuntime::longcs>::max() -
                  ((SharpRuntime::longcs{1} << 55) - 1));              // 2^63 - 2^55
    EXPECT_EQ(static_cast<SharpRuntime::ulongcs>(BFloat16::FromBits(0x5F80u)),
              std::numeric_limits<SharpRuntime::ulongcs>::max());      // 2^64
    EXPECT_EQ(static_cast<SharpRuntime::ulongcs>(BFloat16::FromBits(0x5F7Fu)),
              std::numeric_limits<SharpRuntime::ulongcs>::max() -
                  ((SharpRuntime::ulongcs{1} << 56) - 1));             // 2^64 - 2^56

    EXPECT_EQ(static_cast<SharpRuntime::intcs>(BFloat16::FromBits(0x4F00u)),
              std::numeric_limits<SharpRuntime::intcs>::max());        // 2^31
    EXPECT_EQ(static_cast<SharpRuntime::intcs>(BFloat16::FromBits(0x4EFFu)),
              std::numeric_limits<SharpRuntime::intcs>::max() -
                  ((SharpRuntime::intcs{1} << 23) - 1));               // 2^31 - 2^23
    EXPECT_EQ(static_cast<SharpRuntime::intcs>(BFloat16::FromBits(0xCF00u)),
              std::numeric_limits<SharpRuntime::intcs>::lowest());     // -2^31
    EXPECT_EQ(static_cast<SharpRuntime::intcs>(BFloat16::FromBits(0xCF01u)),
              std::numeric_limits<SharpRuntime::intcs>::lowest());     // below -2^31
    EXPECT_EQ(static_cast<SharpRuntime::uintcs>(BFloat16::FromBits(0x4F80u)),
              std::numeric_limits<SharpRuntime::uintcs>::max());       // 2^32
    EXPECT_EQ(static_cast<SharpRuntime::uintcs>(BFloat16::FromBits(0x4F7Fu)),
              std::numeric_limits<SharpRuntime::uintcs>::max() -
                  ((SharpRuntime::uintcs{1} << 24) - 1));              // 2^32 - 2^24
    EXPECT_EQ(static_cast<SharpRuntime::intcs>(BFloat16::FromBits(0x7F81u)), 0)
        << "a positive signalling NaN also maps to zero";
}

TEST(Fix2384Unit3, EveryFromConversionIsExplicit) {
    // .NET's `from Half` conversions are all `explicit`, and so are these -- a Half must never
    // silently become an integer in arithmetic or overload resolution. Dependent parameters, per
    // the #2299 gcc trap.
    using System::Half;
    using System::Numerics::BFloat16;
    static_assert(!std::is_convertible_v<Half, SharpRuntime::intcs>);
    static_assert(!std::is_convertible_v<Half, SharpRuntime::longcs>);
    static_assert(!std::is_convertible_v<Half, float>);
    static_assert(std::is_constructible_v<SharpRuntime::intcs, Half>);
    static_assert(std::is_constructible_v<SharpRuntime::longcs, Half>);
    static_assert(!std::is_convertible_v<BFloat16, SharpRuntime::intcs>);
    static_assert(std::is_constructible_v<SharpRuntime::intcs, BFloat16>);
    SUCCEED();
}

TEST(Fix2395, ABitPatternAndANumberNowHaveDifferentSpellings) {
    // INVERTED BY #2395 (2026-08-19). Its predecessor,
    // Decl2384Unit3/Decl2395_TheToDirectionWouldHijackEveryIntegerLiteral, pinned the collision
    // as a live demonstration: `Half(uint16_t)` was the RAW BIT PATTERN, and .NET spends that
    // signature on `explicit operator Half(ushort)`, which is `(Half)(float)value` -- a NUMBER.
    //
    // THE DANGEROUS PART OF THIS MIGRATION IS THAT IT COMPILES EITHER WAY. `Half(0x3C00)` is
    // valid under both readings and means 1.0 under one and 15360.0 under the other, so an
    // unmigrated site changes meaning SILENTLY. That is why the rename landed FIRST, with no
    // value-taking constructor present, so the compiler had to name every site; and why this
    // case asserts MEANING rather than constructibility.
    using System::Half;
    using System::Numerics::BFloat16;

    // The bit pattern.
    EXPECT_EQ(Half::FromBits(0x3C00).bits, 0x3C00u);
    EXPECT_FLOAT_EQ(Half::FromBits(0x3C00).ToSingle(), 1.0f);
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::FromBits(0x3F80)), 0x3F80u);
    EXPECT_FLOAT_EQ(static_cast<float>(BFloat16::FromBits(0x3F80)), 1.0f);

    // The number, through the signature the rename freed.
    EXPECT_FLOAT_EQ(Half(static_cast<SharpRuntime::ushortcs>(0x3C00)).ToSingle(), 15360.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(BFloat16(static_cast<SharpRuntime::ushortcs>(0x3F80))),
                    16256.0f);

    // The port's OWN constants are the sites that would have broken silently, so they are
    // asserted here and not only in their own file.
    EXPECT_EQ(Half::One.bits, 0x3C00u);
    EXPECT_EQ(Half::MaxValue.bits, 0x7BFFu);
    EXPECT_EQ(Half::NegativeInfinity.bits, 0xFC00u);
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::One()), 0x3F80u);
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16::NegativeInfinity()), 0xFF80u);
}

TEST(Fix2395, ByteAndSByteAreExplicitHereWhereDotNetMakesThemImplicit) {
    using System::Half;
    using System::Numerics::BFloat16;
    // .NET writes `public static implicit operator Half(byte)` (Half.cs:980) and the same for
    // sbyte (:986). This port cannot: C++ permits a standard conversion BEFORE a user-defined
    // one where C# does not, so an implicit converting constructor from bytecs makes EVERY
    // integer argument ambiguous -- measured while landing this. What is lost is the
    // IMPLICITNESS, never the conversion.
    static_assert(!std::is_convertible_v<SharpRuntime::bytecs, Half>);
    static_assert(!std::is_convertible_v<SharpRuntime::sbytecs, Half>);
    static_assert(std::is_constructible_v<Half, SharpRuntime::bytecs>);
    static_assert(std::is_constructible_v<Half, SharpRuntime::sbytecs>);
    static_assert(!std::is_convertible_v<SharpRuntime::bytecs, BFloat16>);
    static_assert(!std::is_convertible_v<SharpRuntime::sbytecs, BFloat16>);
    static_assert(std::is_constructible_v<BFloat16, SharpRuntime::bytecs>);
    static_assert(std::is_constructible_v<BFloat16, SharpRuntime::sbytecs>);

    // And they convert the VALUE, which is the half that must survive being explicit.
    EXPECT_FLOAT_EQ(Half(static_cast<SharpRuntime::sbytecs>(3)).ToSingle(), 3.0f)
        << "before #2395 this compiled and meant the BITS 0x0003";
    EXPECT_FLOAT_EQ(static_cast<float>(BFloat16(static_cast<SharpRuntime::bytecs>(3))), 3.0f);
}

TEST(Fix2395, EveryToConversionIsExplicitAndBothTypesMoveInStep) {
    using System::Half;
    using System::Numerics::BFloat16;
    // #2340: the two 16-bit floats move in step in SURFACE. Each type gets what .NET gives it,
    // which is why this asserts the SAME scalar list on both rather than identical bodies.
    static_assert(std::is_constructible_v<Half, SharpRuntime::charcs>);
    static_assert(std::is_constructible_v<Half, SharpRuntime::shortcs>);
    static_assert(std::is_constructible_v<Half, SharpRuntime::intcs>);
    static_assert(std::is_constructible_v<Half, SharpRuntime::uintcs>);
    static_assert(std::is_constructible_v<Half, SharpRuntime::longcs>);
    static_assert(std::is_constructible_v<Half, SharpRuntime::ulongcs>);
    static_assert(std::is_constructible_v<Half, float>);
    static_assert(std::is_constructible_v<Half, double>);
    static_assert(!std::is_convertible_v<SharpRuntime::intcs, Half>);
    static_assert(!std::is_convertible_v<float, Half>);
    static_assert(!std::is_convertible_v<double, Half>);

    static_assert(std::is_constructible_v<BFloat16, SharpRuntime::charcs>);
    static_assert(std::is_constructible_v<BFloat16, SharpRuntime::shortcs>);
    static_assert(std::is_constructible_v<BFloat16, SharpRuntime::intcs>);
    static_assert(std::is_constructible_v<BFloat16, SharpRuntime::uintcs>);
    static_assert(std::is_constructible_v<BFloat16, SharpRuntime::longcs>);
    static_assert(std::is_constructible_v<BFloat16, SharpRuntime::ulongcs>);
    static_assert(std::is_constructible_v<BFloat16, float>);
    static_assert(std::is_constructible_v<BFloat16, double>);
    static_assert(!std::is_convertible_v<SharpRuntime::intcs, BFloat16>);
    static_assert(!std::is_convertible_v<double, BFloat16>);

    // Neither type declares Decimal, Int128, nint or nuint IN EITHER DIRECTION. Pinned so that a
    // later unit "completing" one direction has to justify the asymmetry.
    static_assert(!std::is_constructible_v<Half, System::Decimal>);
    static_assert(!std::is_constructible_v<BFloat16, System::Decimal>);

    EXPECT_FLOAT_EQ(Half(static_cast<SharpRuntime::intcs>(-3)).ToSingle(), -3.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(BFloat16(static_cast<SharpRuntime::longcs>(-3))), -3.0f);
}

TEST(Fix2395, BFloat16RoundsIntegersDirectlyWhereHalfGoesThroughFloat) {
    using System::Half;
    using System::Numerics::BFloat16;
    // .NET writes `(BFloat16)(float)value` for short/ushort but RoundFromSigned/RoundFromUnsigned
    // for int/long/uint/ulong (BFloat16.cs:560-646), and Half gets `(Half)(float)value` for ALL
    // of them. The reason is arithmetic, not style: BFloat16 keeps only 8 significand bits, so a
    // 32- or 64-bit integer routed through a 24-bit float ROUNDS TWICE.
    //
    // This is the input that proves the port took .NET's path and not the easy one. Checked by
    // hand: 1119879149 = 1.0429... x 2^30, so the significand field is 5 and the biased exponent
    // 157, giving 0x4E85. The float round-trip gives 0x4E86 -- one ulp out.
    const SharpRuntime::intcs doubleRounded = 1119879149;
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16(doubleRounded)), 0x4E85u)
        << "#2395: BFloat16's integer conversions must round ONCE";
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16(static_cast<float>(doubleRounded))), 0x4E86u)
        << "...and this is what routing through float would have produced";

    // THE 64-BIT PATH IS A SEPARATE TEMPLATE INSTANTIATION and needs its own witness -- a
    // mutation routing only `long` through float went uncaught until this row existed.
    const SharpRuntime::longcs doubleRounded64 = 3485786034122340516LL;
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16(doubleRounded64)), 0x5E41u);
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16(static_cast<float>(doubleRounded64))), 0x5E42u)
        << "...and this is the float route, again one ulp out";

    // ROUND-HALF-TO-EVEN, NOT ROUND-HALF-AWAY. 65792 is 1.0000000_1 x 2^16: the discarded part is
    // EXACTLY half and the kept significand is EVEN, so the tie must round DOWN. Every ordinary
    // value agrees under both rules, so without this row a mutation deleting .NET's
    // "when upper is even, the midpoint ties to no increment" adjustment passes everything.
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16(static_cast<SharpRuntime::intcs>(65792))), 0x4780u)
        << "#2395: an exact tie above an even significand rounds DOWN; away-from-zero gives 0x4781";
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16(static_cast<SharpRuntime::intcs>(65536))), 0x4780u)
        << "...the value it ties down TO";
    // ...and a tie above an ODD significand rounds UP, so the rule is not simply "always down".
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16(static_cast<SharpRuntime::intcs>(66816))), 0x4782u);

    // Probed over 400,000 random values of each width: 4 int32 and 2 int64 inputs differ. Rare,
    // but every one of them is a wrong answer, and the ordinary values must be untouched.
    for (SharpRuntime::intcs v : {0, 1, -1, 2, 3, 256, -256, 1000000}) {
        EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16(v)),
                  std::bit_cast<uint16_t>(BFloat16(static_cast<float>(v))))
            << "an ordinary value must agree with the float route, v=" << v;
    }
    // Zero is answered directly. .NET relies on C#'s shift-count MASKING to let the zero case
    // fall out of the FPU path; C++ leaves `x << 32` UNDEFINED. No value assertion can catch a
    // mutation removing this guard, because x86 masks the shift count in hardware and the answer
    // still comes out 0x0000 -- it is undefined behaviour that happens to work. Verified under
    // UBSan instead (build-probe/2395_probe3_ubsan.cpp): with the guard removed, the mutated
    // build reports "shift exponent 32 is too large for 32-bit type" at the shift, and the
    // unmutated build is silent.
    EXPECT_EQ(std::bit_cast<uint16_t>(BFloat16(static_cast<SharpRuntime::intcs>(0))), 0x0000u);

    // Half is deliberately NOT given the same treatment, because .NET does not give it one.
    EXPECT_FLOAT_EQ(Half(static_cast<SharpRuntime::intcs>(1119879149)).ToSingle(),
                    Half::FromSingle(static_cast<float>(1119879149)).ToSingle());
}
