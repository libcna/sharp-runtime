// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include "System/ArithmeticException.hpp"
#include "System/MathF.hpp"
#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/IFormatProvider.hpp"
#include "System/Single.hpp"
#include "System/Span.hpp"

namespace System::Numerics {

using SharpRuntime::intcs;

/**
 * Brain Float 16 — a 16-bit floating-point type using the same bit layout as
 * the upper 16 bits of a 32-bit IEEE 754 float (1 sign, 8 exponent, 7 mantissa bits).
 *
 * BFloat16 has the same dynamic range as float32 but less precision than float16.
 * Arithmetic is performed by promoting to float, computing, then converting back
 * with IEEE round-to-nearest-even.
 *
 * @note Status: Partial. The deviations below are **declared, not accidental** — their
 *   absence being undeclared is what let SR-AUD-176 be raised as a defect, and #2340
 *   measured that this port had already decided every one of them for `System::Half`,
 *   the other 16-bit float here (`Half.hpp`'s own status block). The two types are kept
 *   consistent with each other on purpose; reopening any line below must move BOTH, which
 *   is ticket **#2383**.
 *   - The ~40 explicit/implicit conversion operators .NET defines to and from every other
 *     numeric primitive (byte, sbyte, short, ushort, uint, ulong, nint, nuint, char,
 *     decimal, Int128, UInt128, plus their `checked` variants — a C# 11 language feature
 *     with no C++ equivalent) are not ported; every one is defined in .NET as a trivial
 *     `(BFloat16)(float)value` round-trip, so a caller writes the same thing in one line
 *     through the `float` conversion this type already has.
 *   - The static math surface mirroring `System.MathF` (`Abs`, `Sign`, `Clamp`, `CopySign`,
 *     `MaxMagnitude`/`MinMagnitude`, `BitIncrement`/`BitDecrement`, and the trigonometric,
 *     exponential, logarithmic, root, hyperbolic and power families) is not ported for the
 *     same reason: widen to `float`, call the existing `System::Single`/`std::` equivalent,
 *     narrow back. Duplicating it here adds no capability, only forwarding boilerplate.
 *   - Generic-math interface conformance is out of scope, consistent with this codebase's
 *     position on C# generic-math machinery everywhere else. .NET's `BFloat16` implements
 *     **36** interfaces and its `ref/` surface is **193** public members; none of the
 *     static-abstract machinery is expressible in C++ as written.
 *   - Formatting and parsing forward to `System::Single`, so they inherit its format and
 *     provider behaviour — including that an `IFormatProvider*` is accepted and ignored.
 */
class BFloat16 {
    uint16_t bits_;

    /**
     * Converts a float to the BFloat16 payload with IEEE round-to-nearest-even,
     * matching current .NET, which passes non-NaN float bits through
     * RoundMidpointToEven(bits, 16) before taking the upper half (SR-AUD-175).
     *
     * Simply shifting right by 16 truncates instead, which biased every
     * conversion — and every arithmetic result, since arithmetic constructs
     * through this path — toward zero, and, worse, turned a NaN whose payload
     * lives only in the discarded low bits into an infinity: 0x7F800001 is a
     * signalling NaN whose upper half is exactly the +Infinity pattern.
     *
     * NaN is therefore handled before rounding rather than by it. Adding a
     * rounding bias to a NaN can carry into the exponent and produce that same
     * silent infinity, so a NaN keeps its sign and is made quiet directly.
     */
    static uint16_t fromFloat(float f) {
        uint32_t u;
        std::memcpy(&u, &f, 4);
        if ((u & 0x7F800000u) == 0x7F800000u && (u & 0x007FFFFFu) != 0u) {
            // NaN: preserve the sign, set the quiet bit, discard the payload.
            return static_cast<uint16_t>((u >> 16) | 0x0040u);
        }
        // Round half to even: add half an ulp, plus one more when the retained
        // payload is odd, so an exact midpoint lands on the even neighbour. A
        // carry out of the mantissa flows into the exponent on its own, which is
        // also how a finite value at or above (MaxValue + half ulp) becomes an
        // infinity — ordinary IEEE overflow-on-rounding.
        const uint32_t rounded = u + 0x7FFFu + ((u >> 16) & 1u);
        return static_cast<uint16_t>(rounded >> 16);
    }
    static float toFloat(uint16_t bits) {
        uint32_t u = static_cast<uint32_t>(bits) << 16;
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }

public:
    /** Constructs BFloat16 with value 0. */
    constexpr BFloat16() : bits_(0) {}
    /** Constructs BFloat16 from raw 16-bit pattern (no conversion). */
    constexpr explicit BFloat16(uint16_t rawBits) : bits_(rawBits) {}
    /** Converts a float to BFloat16, rounding to nearest with ties to even. */
    explicit BFloat16(float v) : bits_(fromFloat(v)) {}

    /** @return BFloat16 representation of 0. */
    static BFloat16 Zero()             { return BFloat16(uint16_t(0)); }
    /** @return BFloat16 representation of 1. */
    static BFloat16 One()              { return BFloat16(uint16_t(0x3F80u)); }
    /** @return BFloat16 representation of -1. */
    static BFloat16 NegativeOne()      { return BFloat16(uint16_t(0xBF80u)); }
    /** @return The largest finite BFloat16 value (~3.39×10³⁸). */
    static BFloat16 MaxValue()         { return BFloat16(uint16_t(0x7F7Fu)); }
    /** @return The most negative finite BFloat16 value (~−3.39×10³⁸). */
    static BFloat16 MinValue()         { return BFloat16(uint16_t(0xFF7Fu)); }
    /** @return The smallest positive BFloat16 value greater than zero (~9.18×10⁻⁴¹). */
    static BFloat16 Epsilon()          { return BFloat16(uint16_t(0x0001u)); }
    /** @return A BFloat16 Not-a-Number (NaN) value. */
    static BFloat16 NaN()              { return BFloat16(uint16_t(0xFFC0u)); }
    /** @return Positive infinity. */
    static BFloat16 PositiveInfinity() { return BFloat16(uint16_t(0x7F80u)); }
    /** @return Negative infinity. */
    static BFloat16 NegativeInfinity() { return BFloat16(uint16_t(0xFF80u)); }

    /** @return The raw 16-bit bit pattern. */
    [[nodiscard]] uint16_t getBitsProperty() const { return bits_; }

    /** Converts to float (lossless — BFloat16 is a subset of float32). */
    explicit operator float()  const { return toFloat(bits_); }
    /** Converts to double. */
    explicit operator double() const { return static_cast<double>(toFloat(bits_)); }

    /** Addition via float promotion. */
    BFloat16 operator+(const BFloat16& o) const { return BFloat16(toFloat(bits_) + toFloat(o.bits_)); }
    /** Subtraction via float promotion. */
    BFloat16 operator-(const BFloat16& o) const { return BFloat16(toFloat(bits_) - toFloat(o.bits_)); }
    /** Multiplication via float promotion. */
    BFloat16 operator*(const BFloat16& o) const { return BFloat16(toFloat(bits_) * toFloat(o.bits_)); }
    /** Division via float promotion. */
    BFloat16 operator/(const BFloat16& o) const { return BFloat16(toFloat(bits_) / toFloat(o.bits_)); }
    /** Unary negation (flips the sign bit). */
    BFloat16 operator-()                  const { return BFloat16(static_cast<uint16_t>(bits_ ^ 0x8000u)); }

    /** IEEE 754 equality via float promotion (NaN != NaN; +0 == -0). */
    bool operator==(const BFloat16& o) const { return toFloat(bits_) == toFloat(o.bits_); }
    /** IEEE 754 inequality via float promotion (NaN != NaN; +0 == -0). */
    bool operator!=(const BFloat16& o) const { return toFloat(bits_) != toFloat(o.bits_); }
    /** Less-than comparison via float promotion. */
    bool operator< (const BFloat16& o) const { return toFloat(bits_) <  toFloat(o.bits_); }
    /** Less-than-or-equal comparison via float promotion. */
    bool operator<=(const BFloat16& o) const { return toFloat(bits_) <= toFloat(o.bits_); }
    /** Greater-than comparison via float promotion. */
    bool operator> (const BFloat16& o) const { return toFloat(bits_) >  toFloat(o.bits_); }
    /** Greater-than-or-equal comparison via float promotion. */
    bool operator>=(const BFloat16& o) const { return toFloat(bits_) >= toFloat(o.bits_); }

    /** @return True if @p v is a NaN value. */
    static bool IsNaN(BFloat16 v)              { return (v.bits_ & 0x7FFFu) > 0x7F80u; }
    /** @return True if @p v is positive or negative infinity. */
    static bool IsInfinity(BFloat16 v)         { return (v.bits_ & 0x7FFFu) == 0x7F80u; }
    /** @return True if @p v is positive infinity. */
    static bool IsPositiveInfinity(BFloat16 v) { return v.bits_ == 0x7F80u; }
    /** @return True if @p v is negative infinity. */
    static bool IsNegativeInfinity(BFloat16 v) { return v.bits_ == 0xFF80u; }

    /**
     * @return The value formatted as a decimal string using the invariant locale.
     *
     * @par #2382 fixed a defect here that its own ticket did not name
     * This body was a bare `std::to_chars`, which is a **second formatter** beside
     * `System::Single::ToString` -- and the two disagree. Compared exhaustively over all
     * 65,536 bit patterns, **256 of them differed**: the two infinities and all 254 NaNs,
     * where `to_chars` writes C's lowercase `"inf"`/`"-inf"`/`"nan"` and .NET writes
     * `"Infinity"`, `"-Infinity"` and `"NaN"` (`BFloat16.cs:394` is
     * `Number.FormatFloat(...)`, which never produces the lowercase forms).
     *
     * It was found by a MUTATION rather than by a test: dropping `ToString(format)`'s
     * empty-format branch was not caught, which is only possible if the two formatters
     * agree -- and measuring whether they really did is what surfaced the 256 that do not.
     * There is now one formatter, so the question cannot recur.
     */
    [[nodiscard]] std::string ToString() const {
        return System::Single::ToString(toFloat(bits_), "");
    }

    // -----------------------------------------------------------------------------------
    // #2382 (SR-AUD-176 unit A). The fifteen members `System::Half` had and this type did
    // not, plus `IsZero`. Derived from BFloat16.cs, NOT copied from Half.hpp -- see the note
    // on the identity trio below, where copying Half would have been wrong.
    // -----------------------------------------------------------------------------------

    /**
     * @brief Determines whether the value is finite (zero, subnormal, or normal).
     *
     * `BFloat16.cs:150-154`: `(~bits & PositiveInfinityBits) != 0`, i.e. the eight exponent
     * bits are not all set. Written here as the equivalent mask comparison.
     */
    // -----------------------------------------------------------------------------------
    // #2384 unit 1: the sign/magnitude family, moving in step with System::Half.
    //
    // EVERY BODY WAS DERIVED FROM BFloat16.cs SEPARATELY rather than copied from Half, which is
    // #2382's lesson applied rather than quoted: there, copying Half::GetHashCode would have
    // compiled, satisfied the hash contract and returned the WRONG NUMBER, because .NET's
    // BFloat16 delegates its identity trio to float while Half masks to 16 bits. Checked here:
    // for these nine members the two references agree, and the only textual difference is that
    // BFloat16.Clamp calls Math.Clamp where Half.Clamp calls float.Clamp -- same semantics.
    // -----------------------------------------------------------------------------------

    /** @brief The absolute value. .NET: `value._value & ~SignMask` (`BFloat16.cs:1396`) -- a BIT
     *  MASK, not a float round-trip, so a NaN keeps its payload. */
    [[nodiscard]] static BFloat16 Abs(BFloat16 value) noexcept {
        return BFloat16(static_cast<uint16_t>(value.bits_ & 0x7FFFu));
    }

    /** @brief Copies the sign of @p sign onto the magnitude of @p value. .NET:
     *  `BFloat16.cs:1300-1310`, bitwise because -- in its own comment -- the method "is required
     *  to work for all inputs, including NaN". */
    [[nodiscard]] static BFloat16 CopySign(BFloat16 value, BFloat16 sign) noexcept {
        return BFloat16(static_cast<uint16_t>((value.bits_ & 0x7FFFu) | (sign.bits_ & 0x8000u)));
    }

    /** @brief The next representable value greater than @p x. .NET: `BFloat16.cs:1134-1164`.
     *  Edges transcribed: NaN returns itself, -Infinity returns MinValue, +Infinity returns
     *  itself, and **-0.0 returns Epsilon**. */
    [[nodiscard]] static BFloat16 BitIncrement(BFloat16 x) noexcept {
        uint16_t bits = x.bits_;
        if (!IsFinite(x)) {
            return (bits == 0xFF80u) ? MinValue() : x;   // -Infinity -> MinValue
        }
        if (bits == 0x8000u) return Epsilon();           // -0.0 -> Epsilon
        if (IsNegative(x)) --bits; else ++bits;
        return BFloat16(bits);
    }

    /** @brief The next representable value less than @p x. .NET: `BFloat16.cs:1101-1131`.
     *  +Infinity returns MaxValue and **+0.0 returns -Epsilon**. */
    [[nodiscard]] static BFloat16 BitDecrement(BFloat16 x) noexcept {
        uint16_t bits = x.bits_;
        if (!IsFinite(x)) {
            return (bits == 0x7F80u) ? MaxValue() : x;   // +Infinity -> MaxValue
        }
        if (bits == 0x0000u) {
            return BFloat16(static_cast<uint16_t>(Epsilon().bits_ | 0x8000u));  // +0.0 -> -Epsilon
        }
        if (IsNegative(x)) ++bits; else --bits;
        return BFloat16(bits);
    }

    /** @brief Clamps @p value to [@p min, @p max]. .NET: `(BFloat16)Math.Clamp((float)...)`
     *  (`BFloat16.cs:1297`) -- a float round-trip, unlike Abs and CopySign above. */
    [[nodiscard]] static BFloat16 Clamp(BFloat16 value, BFloat16 min, BFloat16 max) {
        return BFloat16(System::MathF::Clamp(toFloat(value.bits_), toFloat(min.bits_),
                                             toFloat(max.bits_)));
    }
    /** @brief The larger of two values, through `float`, as .NET does. */
    [[nodiscard]] static BFloat16 Max(BFloat16 x, BFloat16 y) noexcept {
        return BFloat16(System::MathF::Max(toFloat(x.bits_), toFloat(y.bits_)));
    }
    /** @brief The smaller of two values, through `float`, as .NET does. */
    [[nodiscard]] static BFloat16 Min(BFloat16 x, BFloat16 y) noexcept {
        return BFloat16(System::MathF::Min(toFloat(x.bits_), toFloat(y.bits_)));
    }
    /** @brief The value with the larger magnitude. .NET: `float.MaxMagnitude`
     *  (`BFloat16.cs:1491`). */
    [[nodiscard]] static BFloat16 MaxMagnitude(BFloat16 x, BFloat16 y) noexcept {
        return BFloat16(System::MathF::MaxMagnitude(toFloat(x.bits_), toFloat(y.bits_)));
    }
    /** @brief The value with the smaller magnitude. .NET: `float.MinMagnitude`
     *  (`BFloat16.cs:1519`). */
    [[nodiscard]] static BFloat16 MinMagnitude(BFloat16 x, BFloat16 y) noexcept {
        return BFloat16(System::MathF::MinMagnitude(toFloat(x.bits_), toFloat(y.bits_)));
    }

    // -----------------------------------------------------------------------------------
    // #2384 unit 2b: the transcendental, root, power and angular families.
    //
    // EVERY ONE IS A FLOAT ROUND-TRIP, and that is .NET's own shape rather than a
    // simplification -- `(BFloat16)MathF.Sqrt((float)x)` and friends, verified member by member
    // against BFloat16.cs. So unlike units 1 and 2a, there is no bit-level body here to get wrong.
    //
    // TEN MEMBERS .NET DECLARES ARE ABSENT, AND THAT IS MEASURED RATHER THAN OVERLOOKED:
    // Compound, ExpM1, Exp2M1, Exp10M1, LogP1, Log2P1, Log10P1, Lerp, MultiplyAddEstimate and
    // ClampNative have NO counterpart in this port's System::MathF OR System::Single, so
    // adding them here would mean widening `float`'s surface first -- a different type's
    // public API and a different ticket's scope. See the migration note.
    // -----------------------------------------------------------------------------------

    /** @brief C++ counterpart of .NET `BFloat16.Acos` -- `(BFloat16)MathF.Acos((float)...)`. */
    [[nodiscard]] static BFloat16 Acos(BFloat16 x) noexcept { return BFloat16(System::MathF::Acos(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Acosh` -- `(BFloat16)MathF.Acosh((float)...)`. */
    [[nodiscard]] static BFloat16 Acosh(BFloat16 x) noexcept { return BFloat16(System::MathF::Acosh(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Asin` -- `(BFloat16)MathF.Asin((float)...)`. */
    [[nodiscard]] static BFloat16 Asin(BFloat16 x) noexcept { return BFloat16(System::MathF::Asin(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Asinh` -- `(BFloat16)MathF.Asinh((float)...)`. */
    [[nodiscard]] static BFloat16 Asinh(BFloat16 x) noexcept { return BFloat16(System::MathF::Asinh(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Atan` -- `(BFloat16)MathF.Atan((float)...)`. */
    [[nodiscard]] static BFloat16 Atan(BFloat16 x) noexcept { return BFloat16(System::MathF::Atan(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Atanh` -- `(BFloat16)MathF.Atanh((float)...)`. */
    [[nodiscard]] static BFloat16 Atanh(BFloat16 x) noexcept { return BFloat16(System::MathF::Atanh(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Atan2` -- `(BFloat16)MathF.Atan2((float)...)`. */
    [[nodiscard]] static BFloat16 Atan2(BFloat16 y, BFloat16 x) noexcept { return BFloat16(System::MathF::Atan2(toFloat(y.bits_), toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Cbrt` -- `(BFloat16)MathF.Cbrt((float)...)`. */
    [[nodiscard]] static BFloat16 Cbrt(BFloat16 x) noexcept { return BFloat16(System::MathF::Cbrt(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Cos` -- `(BFloat16)MathF.Cos((float)...)`. */
    [[nodiscard]] static BFloat16 Cos(BFloat16 x) noexcept { return BFloat16(System::MathF::Cos(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Cosh` -- `(BFloat16)MathF.Cosh((float)...)`. */
    [[nodiscard]] static BFloat16 Cosh(BFloat16 x) noexcept { return BFloat16(System::MathF::Cosh(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Exp` -- `(BFloat16)MathF.Exp((float)...)`. */
    [[nodiscard]] static BFloat16 Exp(BFloat16 x) noexcept { return BFloat16(System::MathF::Exp(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Log` -- `(BFloat16)MathF.Log((float)...)`. */
    [[nodiscard]] static BFloat16 Log(BFloat16 x) noexcept { return BFloat16(System::MathF::Log(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Log10` -- `(BFloat16)MathF.Log10((float)...)`. */
    [[nodiscard]] static BFloat16 Log10(BFloat16 x) noexcept { return BFloat16(System::MathF::Log10(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Log2` -- `(BFloat16)MathF.Log2((float)...)`. */
    [[nodiscard]] static BFloat16 Log2(BFloat16 x) noexcept { return BFloat16(System::MathF::Log2(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Pow` -- `(BFloat16)MathF.Pow((float)...)`. */
    [[nodiscard]] static BFloat16 Pow(BFloat16 x, BFloat16 y) noexcept { return BFloat16(System::MathF::Pow(toFloat(x.bits_), toFloat(y.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Sin` -- `(BFloat16)MathF.Sin((float)...)`. */
    [[nodiscard]] static BFloat16 Sin(BFloat16 x) noexcept { return BFloat16(System::MathF::Sin(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Sinh` -- `(BFloat16)MathF.Sinh((float)...)`. */
    [[nodiscard]] static BFloat16 Sinh(BFloat16 x) noexcept { return BFloat16(System::MathF::Sinh(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Sqrt` -- `(BFloat16)MathF.Sqrt((float)...)`. */
    [[nodiscard]] static BFloat16 Sqrt(BFloat16 x) noexcept { return BFloat16(System::MathF::Sqrt(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Tan` -- `(BFloat16)MathF.Tan((float)...)`. */
    [[nodiscard]] static BFloat16 Tan(BFloat16 x) noexcept { return BFloat16(System::MathF::Tan(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Tanh` -- `(BFloat16)MathF.Tanh((float)...)`. */
    [[nodiscard]] static BFloat16 Tanh(BFloat16 x) noexcept { return BFloat16(System::MathF::Tanh(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.ScaleB` -- `(BFloat16)MathF.ScaleB((float)...)`. */
    [[nodiscard]] static BFloat16 ScaleB(BFloat16 x, SharpRuntime::intcs n) noexcept { return BFloat16(System::MathF::ScaleB(toFloat(x.bits_), n)); }
    /** @brief C++ counterpart of .NET `BFloat16.FusedMultiplyAdd` -- `(BFloat16)MathF.FusedMultiplyAdd((float)...)`. */
    [[nodiscard]] static BFloat16 FusedMultiplyAdd(BFloat16 left, BFloat16 right, BFloat16 addend) noexcept { return BFloat16(System::MathF::FusedMultiplyAdd(toFloat(left.bits_), toFloat(right.bits_), toFloat(addend.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.ReciprocalEstimate` -- `(BFloat16)MathF.ReciprocalEstimate((float)...)`. */
    [[nodiscard]] static BFloat16 ReciprocalEstimate(BFloat16 x) noexcept { return BFloat16(System::MathF::ReciprocalEstimate(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.ReciprocalSqrtEstimate` -- `(BFloat16)MathF.ReciprocalSqrtEstimate((float)...)`. */
    [[nodiscard]] static BFloat16 ReciprocalSqrtEstimate(BFloat16 x) noexcept { return BFloat16(System::MathF::ReciprocalSqrtEstimate(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Ieee754Remainder` -- `(BFloat16)MathF.IEEERemainder((float)...)`. */
    [[nodiscard]] static BFloat16 Ieee754Remainder(BFloat16 left, BFloat16 right) noexcept { return BFloat16(System::MathF::IEEERemainder(toFloat(left.bits_), toFloat(right.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Log` -- `(BFloat16)MathF.Log((float)...)`. */
    [[nodiscard]] static BFloat16 Log(BFloat16 x, BFloat16 newBase) noexcept { return BFloat16(System::MathF::Log(toFloat(x.bits_), toFloat(newBase.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.AcosPi` -- `(BFloat16)Single.AcosPi((float)...)`. */
    [[nodiscard]] static BFloat16 AcosPi(BFloat16 x) noexcept { return BFloat16(System::Single::AcosPi(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.AsinPi` -- `(BFloat16)Single.AsinPi((float)...)`. */
    [[nodiscard]] static BFloat16 AsinPi(BFloat16 x) noexcept { return BFloat16(System::Single::AsinPi(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.AtanPi` -- `(BFloat16)Single.AtanPi((float)...)`. */
    [[nodiscard]] static BFloat16 AtanPi(BFloat16 x) noexcept { return BFloat16(System::Single::AtanPi(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Atan2Pi` -- `(BFloat16)Single.Atan2Pi((float)...)`. */
    [[nodiscard]] static BFloat16 Atan2Pi(BFloat16 y, BFloat16 x) noexcept { return BFloat16(System::Single::Atan2Pi(toFloat(y.bits_), toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.CosPi` -- `(BFloat16)Single.CosPi((float)...)`. */
    [[nodiscard]] static BFloat16 CosPi(BFloat16 x) noexcept { return BFloat16(System::Single::CosPi(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.SinPi` -- `(BFloat16)Single.SinPi((float)...)`. */
    [[nodiscard]] static BFloat16 SinPi(BFloat16 x) noexcept { return BFloat16(System::Single::SinPi(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.TanPi` -- `(BFloat16)Single.TanPi((float)...)`. */
    [[nodiscard]] static BFloat16 TanPi(BFloat16 x) noexcept { return BFloat16(System::Single::TanPi(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.DegreesToRadians` -- `(BFloat16)Single.DegreesToRadians((float)...)`. */
    [[nodiscard]] static BFloat16 DegreesToRadians(BFloat16 degrees) noexcept { return BFloat16(System::Single::DegreesToRadians(toFloat(degrees.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.RadiansToDegrees` -- `(BFloat16)Single.RadiansToDegrees((float)...)`. */
    [[nodiscard]] static BFloat16 RadiansToDegrees(BFloat16 radians) noexcept { return BFloat16(System::Single::RadiansToDegrees(toFloat(radians.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Exp10` -- `(BFloat16)Single.Exp10((float)...)`. */
    [[nodiscard]] static BFloat16 Exp10(BFloat16 x) noexcept { return BFloat16(System::Single::Exp10(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Exp2` -- `(BFloat16)Single.Exp2((float)...)`. */
    [[nodiscard]] static BFloat16 Exp2(BFloat16 x) noexcept { return BFloat16(System::Single::Exp2(toFloat(x.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.Hypot` -- `(BFloat16)Single.Hypot((float)...)`. */
    [[nodiscard]] static BFloat16 Hypot(BFloat16 x, BFloat16 y) noexcept { return BFloat16(System::Single::Hypot(toFloat(x.bits_), toFloat(y.bits_))); }
    /** @brief C++ counterpart of .NET `BFloat16.RootN` -- `(BFloat16)Single.RootN((float)...)`. */
    [[nodiscard]] static BFloat16 RootN(BFloat16 x, SharpRuntime::intcs n) noexcept { return BFloat16(System::Single::RootN(toFloat(x.bits_), n)); }

    // -----------------------------------------------------------------------------------
    // #2384 unit 2a: rounding, Sign, and the IEEE 754:2019 *Number family.
    //
    // NOTE WHAT IS ABSENT AND WHY. .NET declares MaxNative, MinNative, ClampNative and
    // MultiplyAddEstimate on Half ONLY -- measured by diffing the two ref surfaces. So #2340's
    // in-step rule means EACH TYPE GETS WHAT .NET GIVES IT, not that the two surfaces are
    // identical, and the four members missing here are missing deliberately.
    // -----------------------------------------------------------------------------------

    /** @brief The smallest integral value >= @p x. .NET: `BFloat16.cs`, `MathF.Ceiling` round-trip. */
    [[nodiscard]] static BFloat16 Ceiling(BFloat16 x) noexcept {
        return BFloat16(System::MathF::Ceiling(toFloat(x.bits_)));
    }
    /** @brief The largest integral value <= @p x. */
    [[nodiscard]] static BFloat16 Floor(BFloat16 x) noexcept {
        return BFloat16(System::MathF::Floor(toFloat(x.bits_)));
    }
    /** @brief Rounds to the nearest integral value, ties to even. */
    [[nodiscard]] static BFloat16 Round(BFloat16 x) noexcept {
        return BFloat16(System::MathF::Round(toFloat(x.bits_)));
    }
    /** @brief Rounds to @p digits fractional digits. */
    [[nodiscard]] static BFloat16 Round(BFloat16 x, SharpRuntime::intcs digits) {
        return BFloat16(System::MathF::Round(toFloat(x.bits_), digits));
    }
    /** @brief The integral part of @p x. */
    [[nodiscard]] static BFloat16 Truncate(BFloat16 x) noexcept {
        return BFloat16(System::MathF::Truncate(toFloat(x.bits_)));
    }

    /**
     * @brief The sign of @p value: -1, 0 or +1.
     * @throws System::ArithmeticException if @p value is NaN, as .NET does.
     * @note `Sign(-0.0)` is **0** -- .NET tests IsZero BEFORE IsNegative.
     */
    [[nodiscard]] static SharpRuntime::intcs Sign(BFloat16 value) {
        if (IsNaN(value)) throw System::ArithmeticException(
            "Function does not accept floating point Not-a-Number values.");
        if ((value.bits_ & 0x7FFFu) == 0u) return 0;
        return IsNegative(value) ? -1 : 1;
    }

    /** @brief IEEE 754:2019 `maximumNumber` -- does NOT propagate NaN, and treats +0 as larger
     *  than -0. Both differ from @c Max and both are pinned. */
    [[nodiscard]] static BFloat16 MaxNumber(BFloat16 x, BFloat16 y) noexcept {
        if (!(toFloat(x.bits_) == toFloat(y.bits_))) {
            if (!IsNaN(y)) return (toFloat(y.bits_) < toFloat(x.bits_)) ? x : y;
            return x;
        }
        return IsNegative(y) ? x : y;
    }
    /** @brief IEEE 754:2019 `minimumNumber`. See @c MaxNumber. */
    [[nodiscard]] static BFloat16 MinNumber(BFloat16 x, BFloat16 y) noexcept {
        if (!(toFloat(x.bits_) == toFloat(y.bits_))) {
            if (!IsNaN(y)) return (toFloat(x.bits_) < toFloat(y.bits_)) ? x : y;
            return x;
        }
        return IsNegative(x) ? x : y;
    }
    /** @brief IEEE 754:2019 `maximumMagnitudeNumber`. */
    [[nodiscard]] static BFloat16 MaxMagnitudeNumber(BFloat16 x, BFloat16 y) noexcept {
        const float ax = toFloat(Abs(x).bits_);
        const float ay = toFloat(Abs(y).bits_);
        if ((ax > ay) || IsNaN(y)) return x;
        if (ax == ay) return IsNegative(x) ? y : x;
        return y;
    }
    /** @brief IEEE 754:2019 `minimumMagnitudeNumber`. */
    [[nodiscard]] static BFloat16 MinMagnitudeNumber(BFloat16 x, BFloat16 y) noexcept {
        const float ax = toFloat(Abs(x).bits_);
        const float ay = toFloat(Abs(y).bits_);
        if ((ax < ay) || IsNaN(y)) return x;
        if (ax == ay) return IsNegative(x) ? x : y;
        return y;
    }

    [[nodiscard]] static bool IsFinite(BFloat16 v) noexcept {
        return (v.bits_ & 0x7F80u) != 0x7F80u;
    }

    /**
     * @brief Determines whether the value is negative, **including negative zero and NaN**.
     *
     * `BFloat16.cs:181-185`: `(short)(value._value) < 0` — a pure sign-bit test, so
     * `IsNegative(-0.0f)` and a negative NaN are both `true`. It is deliberately not
     * `value < 0`.
     */
    [[nodiscard]] static bool IsNegative(BFloat16 v) noexcept {
        return (v.bits_ & 0x8000u) != 0u;
    }

    /**
     * @brief Determines whether the value is normal (finite, but not zero or subnormal).
     *
     * `BFloat16.cs:197-201`, whose unsigned-wraparound form
     * `(ushort)(abs - SmallestNormalBits) < (PositiveInfinityBits - SmallestNormalBits)` is
     * a range test on `[0x0080, 0x7F80)`. `SmallestNormalBits` is `0x0080` (`:76`): eight
     * exponent bits with a seven-bit trailing significand, not `Half`'s `0x0400`.
     */
    [[nodiscard]] static bool IsNormal(BFloat16 v) noexcept {
        const uint16_t abs = static_cast<uint16_t>(v.bits_ & 0x7FFFu);
        return abs >= 0x0080u && abs < 0x7F80u;
    }

    /**
     * @brief Determines whether the value is subnormal (finite, but not zero or normal).
     *
     * `BFloat16.cs:213-217`: `(ushort)(abs - 1) < MaxTrailingSignificand`, with
     * `MaxTrailingSignificand == 0x7F` (`:52`), i.e. `abs` in `[1, 0x7F]`.
     */
    [[nodiscard]] static bool IsSubnormal(BFloat16 v) noexcept {
        const uint16_t abs = static_cast<uint16_t>(v.bits_ & 0x7FFFu);
        return abs >= 1u && abs <= 0x007Fu;
    }

    /**
     * @brief Determines whether the value is positive or negative zero.
     *
     * `BFloat16.cs:220-224`. This is the ONE member here that `System::Half` does not have,
     * and including it is following the reference rather than exceeding #2340's
     * decomposition: .NET's `BFloat16` declares `IsZero` **public**, while .NET's `Half`
     * exposes it only through the generic-math interface this port does not implement.
     */
    [[nodiscard]] static bool IsZero(BFloat16 v) noexcept {
        return (v.bits_ & 0x7FFFu) == 0u;
    }

    // --- ordering and identity ---------------------------------------------------------
    //
    // ALL THREE DELEGATE TO `float`, AND THAT IS THE DERIVATION RATHER THAN A SHORTCUT.
    // .NET's are `((float)this).CompareTo((float)other)`, `((float)this).Equals((float)
    // other)` and `((float)this).GetHashCode()` (BFloat16.cs:354, 379, 389). This port's
    // `System::Half` instead implements its own bit-level versions, and COPYING THEM HERE
    // WOULD HAVE BEEN WRONG: `Half::GetHashCode` masks to 16 bits and returns a value in
    // `[0, 0xFFFF]`, whereas .NET's `BFloat16` hash is `float`'s, a 32-bit value derived
    // from the WIDENED number. #2340's decomposition says to follow `Half`'s *line* -- what
    // surface a 16-bit float in this port carries -- not to copy `Half`'s *bodies*.

    /**
     * @brief Compares this value to @p other. `BFloat16.cs:354`.
     * @return Negative, zero or positive. NaN sorts before every number and equals itself,
     *         which is `System::Single::CompareTo`'s contract and .NET's.
     */
    [[nodiscard]] intcs CompareTo(const BFloat16& other) const noexcept {
        return System::Single::CompareTo(toFloat(bits_), toFloat(other.bits_));
    }

    /**
     * @brief Determines whether this instance equals @p other. `BFloat16.cs:379`.
     *
     * Unlike `operator==`, **NaN equals NaN** here and `+0 == -0`, because that is what
     * `float.Equals(float)` does. Keeping the two apart is the whole reason .NET has both.
     */
    [[nodiscard]] bool Equals(const BFloat16& other) const noexcept {
        return System::Single::Equals(toFloat(bits_), toFloat(other.bits_));
    }

    /** @brief Returns a hash code for this value. `BFloat16.cs:389` — `float`'s hash. */
    [[nodiscard]] intcs GetHashCode() const noexcept {
        return System::Single::GetHashCode(toFloat(bits_));
    }

    // --- parsing -----------------------------------------------------------------------

    /**
     * @brief Parses @p s into a `BFloat16`. `BFloat16.cs:231`.
     * @throws System::FormatException if @p s is not a valid floating-point literal.
     */
    [[nodiscard]] static BFloat16 Parse(const std::string& s) {
        return BFloat16(System::Single::Parse(s));
    }

    /** @brief `BFloat16.cs:247`. The provider is accepted and ignored — see the class note. */
    [[nodiscard]] static BFloat16 Parse(const std::string& s, const System::IFormatProvider* provider) {
        (void)provider;
        return Parse(s);
    }

    /**
     * @brief Tries to parse @p s without throwing. `BFloat16.cs:284`.
     *
     * On failure @p result is set to **zero**, matching .NET's `out` contract for every
     * `TryParse` in this family — not left untouched.
     */
    static bool TryParse(const std::string& s, BFloat16& result) noexcept {
        float f = 0.0f;
        if (!System::Single::TryParse(s, f)) { result = BFloat16(); return false; }
        result = BFloat16(f);
        return true;
    }

    /** @brief `BFloat16.cs:1949`. The provider is accepted and ignored. */
    static bool TryParse(const std::string& s, const System::IFormatProvider* provider,
                         BFloat16& result) noexcept {
        (void)provider;
        return TryParse(s, result);
    }

    // --- formatting --------------------------------------------------------------------

    /**
     * @brief Returns a string representation using the given numeric format
     *        (`"F2"`, `"E3"`, `"G"`, `"R"`). `BFloat16.cs:399`.
     *
     * TWO GUARDS THAT `System::Half`'s COUNTERPART HAS ARE DELETED HERE RATHER THAN COPIED,
     * and each was deleted because a mutation removing it could not be caught:
     *
     *  - naming the three non-finite values before consulting the format. Measured over all
     *    five format kinds, `System::Single::ToString` already answers `"NaN"`,
     *    `"Infinity"` and `"-Infinity"` for every one of them, so the guard was dead;
     *  - an `if (format.empty()) return ToString();` short-circuit. Once `ToString()` became
     *    `Single::ToString(value, "")` (see its own note), the two branches are the *same
     *    call*, so this was an equivalence rather than a check.
     *
     * The first mutation is what led to the second, and the second to the `ToString()`
     * repair -- asking whether the branch really was redundant is what surfaced the 256 bit
     * patterns on which the type's two formatters disagreed.
     */
    [[nodiscard]] std::string ToString(const std::string& format) const {
        return System::Single::ToString(toFloat(bits_), format);
    }

    /** @brief `BFloat16.cs:407`. The provider is accepted and ignored. */
    [[nodiscard]] std::string ToString(const System::IFormatProvider* provider) const {
        (void)provider;
        return ToString();
    }

    /** @brief `BFloat16.cs:415`. The provider is accepted and ignored. */
    [[nodiscard]] std::string ToString(const std::string& format,
                                       const System::IFormatProvider* provider) const {
        (void)provider;
        return ToString(format);
    }

    /**
     * @brief Tries to write this value's text into @p destination. `BFloat16.cs:428`.
     *
     * @return `false` with `charsWritten == 0` when the span is too short, writing nothing.
     *         .NET's contract is that a failed `TryFormat` leaves the destination
     *         unspecified and `charsWritten` zero; writing a truncated prefix would be the
     *         more dangerous reading of "unspecified".
     */
    bool TryFormat(Span<char> destination, intcs& charsWritten,
                   const std::string& format = "") const {
        const std::string s = ToString(format);
        if (static_cast<intcs>(s.size()) > destination.getLengthProperty()) {
            charsWritten = 0;
            return false;
        }
        std::copy(s.begin(), s.end(), destination.getPointer());
        charsWritten = static_cast<intcs>(s.size());
        return true;
    }
};

} // namespace System::Numerics
