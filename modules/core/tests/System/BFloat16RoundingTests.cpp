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
#include <array>
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
    const BFloat16 zero      = BFloat16(uint16_t(0x0000));
    const BFloat16 negZero    = BFloat16(uint16_t(0x8000));
    const BFloat16 subnormal  = BFloat16(uint16_t(0x0001));   // the largest-magnitude one is 0x007F
    const BFloat16 maxSubnorm = BFloat16(uint16_t(0x007F));
    const BFloat16 minNormal  = BFloat16(uint16_t(0x0080));
    const BFloat16 one        = BFloat16(uint16_t(0x3F80));
    const BFloat16 inf        = BFloat16(uint16_t(0x7F80));
    const BFloat16 negInf     = BFloat16(uint16_t(0xFF80));
    const BFloat16 nan        = BFloat16(uint16_t(0x7FC0));
    const BFloat16 negNan     = BFloat16(uint16_t(0xFFC0));

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
        const BFloat16 v = BFloat16(raw);
        const int classes = int(BFloat16::IsZero(v)) + int(BFloat16::IsSubnormal(v))
                          + int(BFloat16::IsNormal(v)) + int(!BFloat16::IsFinite(v));
        ASSERT_EQ(classes, 1) << "bit pattern 0x" << std::hex << raw << " is in " << classes;
        if (raw == 0xFFFF) break;
    }
}

TEST(BFloat16SurfaceTests, Fix2382_IsNegativeIsASignBitTestNotAComparison) {
    // BFloat16.cs:181-185 is `(short)(value._value) < 0`. So negative zero and a negative
    // NaN are both negative, which `v < BFloat16()` would report as false for both.
    EXPECT_TRUE(BFloat16::IsNegative(BFloat16(uint16_t(0x8000))));   // -0
    EXPECT_FALSE(BFloat16::IsNegative(BFloat16(uint16_t(0x0000))));  // +0
    EXPECT_TRUE(BFloat16::IsNegative(BFloat16(uint16_t(0xFFC0))));   // -NaN
    EXPECT_FALSE(BFloat16::IsNegative(BFloat16(uint16_t(0x7FC0))));  // +NaN
    EXPECT_TRUE(BFloat16::IsNegative(BFloat16(uint16_t(0xFF80))));   // -Infinity
    EXPECT_TRUE(BFloat16::IsNegative(BFloat16(-1.0f)));
    EXPECT_FALSE(BFloat16::IsNegative(BFloat16(1.0f)));
}

TEST(BFloat16SurfaceTests, Fix2382_TheIdentityTrioIsFloatsNotHalfs) {
    const BFloat16 nan  = BFloat16(uint16_t(0x7FC0));
    const BFloat16 nan2 = BFloat16(uint16_t(0xFFC0));
    const BFloat16 zero = BFloat16(uint16_t(0x0000));
    const BFloat16 negZero = BFloat16(uint16_t(0x8000));

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
        EXPECT_EQ(BFloat16(uint16_t(0x7FC0)).ToString(fmt), "NaN");
        EXPECT_EQ(BFloat16(uint16_t(0x7F80)).ToString(fmt), "Infinity");
        EXPECT_EQ(BFloat16(uint16_t(0xFF80)).ToString(fmt), "-Infinity");
    }
    EXPECT_EQ(BFloat16(uint16_t(0x7FC0)).ToString(), "NaN");
    EXPECT_EQ(BFloat16(uint16_t(0x7F80)).ToString(), "Infinity");
    EXPECT_EQ(BFloat16(uint16_t(0xFF80)).ToString(), "-Infinity");

    // ...and there is now ONE formatter, which is what stops the two drifting apart again:
    // every bit pattern formats identically through the default and the empty format.
    for (uint32_t r = 0;; ++r) {
        const BFloat16 v = BFloat16(static_cast<uint16_t>(r));
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

TEST(BFloat16SurfaceTests, Fix2382_TheDeclinedSurfaceIsDECLAREDNotMerelyAbsent) {
    // The point of the header's @note block is that the absence is a decision. Pinned here
    // as a statement rather than prose: none of the #2383 surface exists on this type, and
    // if one of them is added the ticket that adds it must move `Half` in the same change.
    // The parameter MUST be dependent. gcc evaluates a non-dependent `requires` eagerly and
    // errors on the missing name instead of yielding false -- measured on ticket #2299,
    // recorded in CLAUDE.md, and hit again writing this case. `T` is what makes it a
    // substitution failure rather than a hard error.
    static_assert(!HasAbs<BFloat16>,
                  "#2383 landed on BFloat16 -- it must move System::Half too.");
    static_assert(!HasBitIncrement<BFloat16>,
                  "#2383 landed on BFloat16 -- it must move System::Half too.");
    static_assert(!HasCopySign<BFloat16>,
                  "#2383 landed on BFloat16 -- it must move System::Half too.");
    static_assert(!HasSqrt<BFloat16>,
                  "#2383 landed on BFloat16 -- it must move System::Half too.");
    // ...and the members #2382 DID add exist, so the two halves of this pin cannot both be
    // satisfied by the type simply failing to compile.
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
