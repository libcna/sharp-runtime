// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include "System/ArithmeticException.hpp"
#include "System/MathF.hpp"
#include <cstring>
#include <string>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <limits>
#include <algorithm>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Single.hpp"
#include "System/Span.hpp"

namespace System {

    using SharpRuntime::intcs;
    using SharpRuntime::longcs;

    class IFormatProvider;

    /**
     * @brief Represents a half-precision (16-bit, IEEE 754 binary16) floating-point number.
     *
     * C++ counterpart of .NET System.Half.
     *
     * @note Status: Partial. Deviations from .NET:
     *   - The ~40 explicit/implicit conversion operators .NET defines to/from every other
     *     numeric primitive (byte, sbyte, short, ushort, uint, ulong, nint, nuint, char,
     *     decimal, Int128, UInt128, plus their `checked` variants — a C# 11 language
     *     feature with no C++ equivalent) are not ported; every one of them is defined in
     *     .NET as a trivial `(Half)(float)value` round-trip, so callers can do the same
     *     via ToSingle()/FromSingle() in one line. float and double are ported directly.
     *   - The ~40 static math functions mirroring `System.MathF` (Exp, Log, Sin, Cos, Sqrt,
     *     Pow, Round, Ceiling, Floor, Truncate, Lerp, FusedMultiplyAdd, BitIncrement/
     *     Decrement, ReciprocalEstimate, ...) are not ported for the same reason: widen via
     *     ToSingle(), call the existing System::Single/std::/MathF equivalent, narrow back
     *     via FromSingle(). Duplicating the entire float math surface as Half overloads
     *     adds no capability, only near-identical forwarding boilerplate.
     *   - Generic math interface conformance (INumber&lt;Half&gt;, IFloatingPointIeee754&lt;Half&gt;,
     *     IMinMaxValue&lt;Half&gt;, etc.) is out of scope, consistent with this codebase's
     *     position on C# generic-math machinery elsewhere.
     *   - FromSingle()/ToSingle() implement IEEE 754 round-to-nearest-even exactly as .NET's
     *     `(Half)float` / `(float)Half` operators do (verified against known bit patterns:
     *     PositiveOneBits, MaxValueBits, Epsilon, NaN, E/Pi/Tau). FromDouble() narrows via
     *     an intermediate float rounding rather than rounding directly from double to half,
     *     which can theoretically double-round differently right at an exact tie boundary —
     *     an extremely narrow, low-impact edge case.
     *   - NaN payload bits are not preserved bit-for-bit through FromSingle()/FromDouble();
     *     a canonical quiet NaN (sign preserved) is produced instead, matching all
     *     NaN-producing entry points' externally observable behavior (IsNaN, comparisons).
     */
    struct Half {
        /** @brief The raw IEEE 754 half-precision bit pattern. */
        uint16_t bits = 0;

        /** @brief Initializes a new Half with zero value. */
        Half() = default;
        /** @brief Initializes a new Half from a raw 16-bit bit pattern. */
        explicit Half(uint16_t rawBits) : bits(rawBits) {}

        /** @brief Converts a 32-bit float to the nearest representable Half (round to nearest, ties to even). */
        [[nodiscard]] static Half FromSingle(float value) noexcept {
            uint32_t f;
            std::memcpy(&f, &value, sizeof(f));
            const uint32_t sign16 = (f >> 16) & 0x8000u;
            const uint32_t absF = f & 0x7FFFFFFFu;

            if (absF > 0x7F800000u) {
                // NaN: canonical quiet NaN, sign preserved. See class-level deviation note.
                return Half(static_cast<uint16_t>(sign16 | 0x7E00u));
            }
            if (absF >= 0x7F800000u) {
                return Half(static_cast<uint16_t>(sign16 | 0x7C00u)); // Infinity
            }
            if (absF == 0u) {
                return Half(static_cast<uint16_t>(sign16)); // signed zero
            }
            if (absF < 0x00800000u) {
                // Float subnormal input: far smaller than Half's smallest subnormal -> zero.
                return Half(static_cast<uint16_t>(sign16));
            }

            const int32_t exp = static_cast<int32_t>((absF >> 23) & 0xFFu) - 127;
            const uint32_t significand = (absF & 0x7FFFFFu) | 0x800000u; // 24-bit, implicit bit set

            const int32_t targetExp = exp + 15;
            if (targetExp >= 31) {
                return Half(static_cast<uint16_t>(sign16 | 0x7C00u)); // definite overflow
            }

            int32_t dropBits;
            int32_t finalExpField;
            if (targetExp <= 0) {
                dropBits = 13 + (1 - targetExp);
                finalExpField = 0;
            } else {
                dropBits = 13;
                finalExpField = targetExp;
            }

            if (dropBits >= 25) {
                return Half(static_cast<uint16_t>(sign16)); // underflows to zero
            }

            const uint32_t dropMask = (1u << dropBits) - 1u;
            const uint32_t lowPart = significand & dropMask;
            const uint32_t halfway = 1u << (dropBits - 1);
            uint32_t rounded = significand >> dropBits;
            if (lowPart > halfway || (lowPart == halfway && (rounded & 1u) != 0u)) {
                rounded += 1u;
            }

            uint32_t mantissaField;
            if (finalExpField == 0) {
                if (rounded == 0x400u) { finalExpField = 1; rounded = 0u; }
                mantissaField = rounded;
            } else {
                if (rounded == 0x800u) { finalExpField += 1; rounded = 0x400u; }
                mantissaField = rounded & 0x3FFu;
            }

            if (finalExpField >= 31) {
                return Half(static_cast<uint16_t>(sign16 | 0x7C00u)); // rounding overflow
            }

            return Half(static_cast<uint16_t>(sign16 | (static_cast<uint32_t>(finalExpField) << 10) | mantissaField));
        }

        /** @brief Converts this Half to its exactly-equal 32-bit float value (widening is always exact). */
        [[nodiscard]] float ToSingle() const noexcept {
            const uint32_t sign = (static_cast<uint32_t>(bits) << 16) & 0x80000000u;
            const uint32_t exp  = (bits >> 10) & 0x1Fu;
            const uint32_t mant = bits & 0x3FFu;
            uint32_t f;
            if (exp == 0x1Fu) {
                f = sign | 0x7F800000u | (mant << 13); // Inf / NaN
            } else if (exp == 0u) {
                if (mant == 0u) {
                    f = sign; // signed zero
                } else {
                    // Subnormal half -> renormalize into a normal float.
                    uint32_t m = mant;
                    int shift = 0;
                    while ((m & 0x400u) == 0u) { m <<= 1; ++shift; }
                    m &= 0x3FFu;
                    const int unbiasedExp = -14 - shift;
                    f = sign | (static_cast<uint32_t>(unbiasedExp + 127) << 23) | (m << 13);
                }
            } else {
                f = sign | ((exp + 112u) << 23) | (mant << 13); // normal: rebias 15 -> 127
            }
            float result;
            std::memcpy(&result, &f, sizeof(f));
            return result;
        }

        /** @brief Converts a double to the nearest representable Half. */
        [[nodiscard]] static Half FromDouble(double value) noexcept {
            return FromSingle(static_cast<float>(value));
        }

        /** @brief Converts this Half to its exactly-equal double value. */
        [[nodiscard]] double ToDouble() const noexcept { return static_cast<double>(ToSingle()); }

        /** @brief Explicit conversion to a 32-bit float. */
        explicit operator float() const noexcept { return ToSingle(); }
        /** @brief Explicit conversion to a 64-bit double. */
        explicit operator double() const noexcept { return ToDouble(); }

        /** @brief Represents the value zero. C++ counterpart of .NET Half.Zero. */
        static const Half Zero;
        /** @brief Represents the value one. C++ counterpart of .NET Half.One. */
        static const Half One;
        /** @brief Represents the value negative one. C++ counterpart of .NET Half.NegativeOne. */
        static const Half NegativeOne;
        /** @brief Represents negative zero. C++ counterpart of .NET Half.NegativeZero. */
        static const Half NegativeZero;
        /** @brief Represents Not a Number (NaN). C++ counterpart of .NET Half.NaN. */
        static const Half NaN;
        /** @brief Represents positive infinity. C++ counterpart of .NET Half.PositiveInfinity. */
        static const Half PositiveInfinity;
        /** @brief Represents negative infinity. C++ counterpart of .NET Half.NegativeInfinity. */
        static const Half NegativeInfinity;
        // -----------------------------------------------------------------------------------
        // #2384 unit 1: the sign/magnitude family. Every body is DERIVED PER TYPE from the
        // reference rather than copied across, because #2382 measured what copying costs -- its
        // GetHashCode would have compiled, satisfied the hash contract and returned the wrong
        // number. Here the two types agree, and that was CHECKED rather than assumed.
        //
        // Note which of these are bit operations and which are float round-trips: .NET makes that
        // distinction deliberately, and a blanket "forward to float" would be wrong for four of
        // the nine.
        // -----------------------------------------------------------------------------------

        /**
         * @brief Returns the absolute value.
         *
         * C++ counterpart of .NET `Half.Abs(Half)` (`Half.cs:1756`), which is a **bit mask**,
         * `value._value & ~SignMask` -- NOT a float round-trip. That matters for NaN: masking
         * preserves the payload and the quiet bit, where a round-trip through `float` would
         * canonicalise them (see this type's own note on NaN payloads).
         */
        [[nodiscard]] static Half Abs(Half value) noexcept {
            return Half(static_cast<uint16_t>(value.bits & 0x7FFFu));
        }

        /**
         * @brief Copies the sign of @p sign onto the magnitude of @p value.
         *
         * C++ counterpart of .NET `Half.CopySign(Half, Half)` (`Half.cs:1654-1664`). .NET states
         * why it is bitwise in a comment of its own -- *"This method is required to work for all
         * inputs, including NaN, so we operate on the raw bits"* -- so this is transcribed rather
         * than expressed through `std::copysign` on a converted `float`.
         */
        [[nodiscard]] static Half CopySign(Half value, Half sign) noexcept {
            return Half(static_cast<uint16_t>((value.bits & 0x7FFFu) | (sign.bits & 0x8000u)));
        }

        /**
         * @brief Returns the next representable value greater than @p x.
         *
         * C++ counterpart of .NET `Half.BitIncrement(Half)` (`Half.cs:1478-1508`), transcribed
         * including its three non-obvious edges: a NaN returns itself, `-Infinity` returns
         * `MinValue` (not `-MaxValue` by arithmetic), `+Infinity` returns itself, and **-0.0
         * returns `Epsilon`** rather than +0.0.
         */
        [[nodiscard]] static Half BitIncrement(Half x) noexcept {
            uint16_t bits = x.bits;
            if (!IsFinite(x)) {
                // NaN -> NaN, -Infinity -> MinValue, +Infinity -> +Infinity
                return (bits == 0xFC00u) ? MinValue : x;
            }
            if (bits == 0x8000u) return Epsilon;   // -0.0 -> Epsilon
            // Negative values are decremented, positive values incremented.
            if (IsNegative(x)) --bits; else ++bits;
            return Half(bits);
        }

        /**
         * @brief Returns the next representable value less than @p x.
         *
         * C++ counterpart of .NET `Half.BitDecrement(Half)` (`Half.cs:1445-1475`). The mirror of
         * BitIncrement, and its edges mirror too: `+Infinity` returns `MaxValue` and **+0.0
         * returns `-Epsilon`**.
         */
        [[nodiscard]] static Half BitDecrement(Half x) noexcept {
            uint16_t bits = x.bits;
            if (!IsFinite(x)) {
                // NaN -> NaN, +Infinity -> MaxValue, -Infinity -> -Infinity
                return (bits == 0x7C00u) ? MaxValue : x;
            }
            if (bits == 0x0000u) {
                // +0.0 -> -Epsilon
                return Half(static_cast<uint16_t>(Epsilon.bits | 0x8000u));
            }
            if (IsNegative(x)) ++bits; else --bits;
            return Half(bits);
        }

        /**
         * @brief Clamps @p value to the inclusive range [@p min, @p max].
         *
         * C++ counterpart of .NET `Half.Clamp(Half, Half, Half)` (`Half.cs:1641`), which **is** a
         * float round-trip -- `(Half)float.Clamp((float)value, (float)min, (float)max)`.
         * @throws System::ArgumentException if @p min is greater than @p max, matching
         *         `Math.Clamp`'s own contract.
         */
        [[nodiscard]] static Half Clamp(Half value, Half min, Half max);

        /** @brief The larger of two values. .NET: `(Half)float.Max(...)` (`Half.cs:1667`). */
        [[nodiscard]] static Half Max(Half x, Half y) noexcept;
        /** @brief The smaller of two values. .NET's counterpart of Max. */
        [[nodiscard]] static Half Min(Half x, Half y) noexcept;
        /** @brief The value with the larger magnitude. .NET: `(Half)MathF.MaxMagnitude(...)`
         *  (`Half.cs:1851`). */
        [[nodiscard]] static Half MaxMagnitude(Half x, Half y) noexcept;
        /** @brief The value with the smaller magnitude. .NET: `(Half)MathF.MinMagnitude(...)`
         *  (`Half.cs:1879`). */
        [[nodiscard]] static Half MinMagnitude(Half x, Half y) noexcept;

        // -----------------------------------------------------------------------------------
        // #2384 unit 2b: the transcendental, root, power and angular families.
        //
        // EVERY ONE IS A FLOAT ROUND-TRIP, and that is .NET's own shape rather than a
        // simplification -- `(Half)MathF.Sqrt((float)x)` and friends, verified member by member
        // against Half.cs. So unlike units 1 and 2a, there is no bit-level body here to get wrong.
        //
        // TEN MEMBERS .NET DECLARES ARE ABSENT, AND THAT IS MEASURED RATHER THAN OVERLOOKED:
        // Compound, ExpM1, Exp2M1, Exp10M1, LogP1, Log2P1, Log10P1, Lerp, MultiplyAddEstimate and
        // ClampNative have NO counterpart in this port's System::MathF OR System::Single, so
        // adding them here would mean widening `float`'s surface first -- a different type's
        // public API and a different ticket's scope. See the migration note.
        // -----------------------------------------------------------------------------------

        /** @brief C++ counterpart of .NET `Half.Acos` -- `(Half)MathF.Acos((float)...)`. */
        [[nodiscard]] static Half Acos(Half x) noexcept { return FromSingle(System::MathF::Acos(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Acosh` -- `(Half)MathF.Acosh((float)...)`. */
        [[nodiscard]] static Half Acosh(Half x) noexcept { return FromSingle(System::MathF::Acosh(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Asin` -- `(Half)MathF.Asin((float)...)`. */
        [[nodiscard]] static Half Asin(Half x) noexcept { return FromSingle(System::MathF::Asin(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Asinh` -- `(Half)MathF.Asinh((float)...)`. */
        [[nodiscard]] static Half Asinh(Half x) noexcept { return FromSingle(System::MathF::Asinh(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Atan` -- `(Half)MathF.Atan((float)...)`. */
        [[nodiscard]] static Half Atan(Half x) noexcept { return FromSingle(System::MathF::Atan(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Atanh` -- `(Half)MathF.Atanh((float)...)`. */
        [[nodiscard]] static Half Atanh(Half x) noexcept { return FromSingle(System::MathF::Atanh(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Atan2` -- `(Half)MathF.Atan2((float)...)`. */
        [[nodiscard]] static Half Atan2(Half y, Half x) noexcept { return FromSingle(System::MathF::Atan2(y.ToSingle(), x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Cbrt` -- `(Half)MathF.Cbrt((float)...)`. */
        [[nodiscard]] static Half Cbrt(Half x) noexcept { return FromSingle(System::MathF::Cbrt(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Cos` -- `(Half)MathF.Cos((float)...)`. */
        [[nodiscard]] static Half Cos(Half x) noexcept { return FromSingle(System::MathF::Cos(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Cosh` -- `(Half)MathF.Cosh((float)...)`. */
        [[nodiscard]] static Half Cosh(Half x) noexcept { return FromSingle(System::MathF::Cosh(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Exp` -- `(Half)MathF.Exp((float)...)`. */
        [[nodiscard]] static Half Exp(Half x) noexcept { return FromSingle(System::MathF::Exp(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Log` -- `(Half)MathF.Log((float)...)`. */
        [[nodiscard]] static Half Log(Half x) noexcept { return FromSingle(System::MathF::Log(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Log10` -- `(Half)MathF.Log10((float)...)`. */
        [[nodiscard]] static Half Log10(Half x) noexcept { return FromSingle(System::MathF::Log10(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Log2` -- `(Half)MathF.Log2((float)...)`. */
        [[nodiscard]] static Half Log2(Half x) noexcept { return FromSingle(System::MathF::Log2(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Pow` -- `(Half)MathF.Pow((float)...)`. */
        [[nodiscard]] static Half Pow(Half x, Half y) noexcept { return FromSingle(System::MathF::Pow(x.ToSingle(), y.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Sin` -- `(Half)MathF.Sin((float)...)`. */
        [[nodiscard]] static Half Sin(Half x) noexcept { return FromSingle(System::MathF::Sin(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Sinh` -- `(Half)MathF.Sinh((float)...)`. */
        [[nodiscard]] static Half Sinh(Half x) noexcept { return FromSingle(System::MathF::Sinh(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Sqrt` -- `(Half)MathF.Sqrt((float)...)`. */
        [[nodiscard]] static Half Sqrt(Half x) noexcept { return FromSingle(System::MathF::Sqrt(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Tan` -- `(Half)MathF.Tan((float)...)`. */
        [[nodiscard]] static Half Tan(Half x) noexcept { return FromSingle(System::MathF::Tan(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Tanh` -- `(Half)MathF.Tanh((float)...)`. */
        [[nodiscard]] static Half Tanh(Half x) noexcept { return FromSingle(System::MathF::Tanh(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.ScaleB` -- `(Half)MathF.ScaleB((float)...)`. */
        [[nodiscard]] static Half ScaleB(Half x, SharpRuntime::intcs n) noexcept { return FromSingle(System::MathF::ScaleB(x.ToSingle(), n)); }
        /** @brief C++ counterpart of .NET `Half.FusedMultiplyAdd` -- `(Half)MathF.FusedMultiplyAdd((float)...)`. */
        [[nodiscard]] static Half FusedMultiplyAdd(Half left, Half right, Half addend) noexcept { return FromSingle(System::MathF::FusedMultiplyAdd(left.ToSingle(), right.ToSingle(), addend.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.ReciprocalEstimate` -- `(Half)MathF.ReciprocalEstimate((float)...)`. */
        [[nodiscard]] static Half ReciprocalEstimate(Half x) noexcept { return FromSingle(System::MathF::ReciprocalEstimate(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.ReciprocalSqrtEstimate` -- `(Half)MathF.ReciprocalSqrtEstimate((float)...)`. */
        [[nodiscard]] static Half ReciprocalSqrtEstimate(Half x) noexcept { return FromSingle(System::MathF::ReciprocalSqrtEstimate(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Ieee754Remainder` -- `(Half)MathF.IEEERemainder((float)...)`. */
        [[nodiscard]] static Half Ieee754Remainder(Half left, Half right) noexcept { return FromSingle(System::MathF::IEEERemainder(left.ToSingle(), right.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Log` -- `(Half)MathF.Log((float)...)`. */
        [[nodiscard]] static Half Log(Half x, Half newBase) noexcept { return FromSingle(System::MathF::Log(x.ToSingle(), newBase.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.AcosPi` -- `(Half)Single.AcosPi((float)...)`. */
        [[nodiscard]] static Half AcosPi(Half x) noexcept { return FromSingle(System::Single::AcosPi(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.AsinPi` -- `(Half)Single.AsinPi((float)...)`. */
        [[nodiscard]] static Half AsinPi(Half x) noexcept { return FromSingle(System::Single::AsinPi(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.AtanPi` -- `(Half)Single.AtanPi((float)...)`. */
        [[nodiscard]] static Half AtanPi(Half x) noexcept { return FromSingle(System::Single::AtanPi(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Atan2Pi` -- `(Half)Single.Atan2Pi((float)...)`. */
        [[nodiscard]] static Half Atan2Pi(Half y, Half x) noexcept { return FromSingle(System::Single::Atan2Pi(y.ToSingle(), x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.CosPi` -- `(Half)Single.CosPi((float)...)`. */
        [[nodiscard]] static Half CosPi(Half x) noexcept { return FromSingle(System::Single::CosPi(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.SinPi` -- `(Half)Single.SinPi((float)...)`. */
        [[nodiscard]] static Half SinPi(Half x) noexcept { return FromSingle(System::Single::SinPi(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.TanPi` -- `(Half)Single.TanPi((float)...)`. */
        [[nodiscard]] static Half TanPi(Half x) noexcept { return FromSingle(System::Single::TanPi(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.DegreesToRadians` -- `(Half)Single.DegreesToRadians((float)...)`. */
        [[nodiscard]] static Half DegreesToRadians(Half degrees) noexcept { return FromSingle(System::Single::DegreesToRadians(degrees.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.RadiansToDegrees` -- `(Half)Single.RadiansToDegrees((float)...)`. */
        [[nodiscard]] static Half RadiansToDegrees(Half radians) noexcept { return FromSingle(System::Single::RadiansToDegrees(radians.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Exp10` -- `(Half)Single.Exp10((float)...)`. */
        [[nodiscard]] static Half Exp10(Half x) noexcept { return FromSingle(System::Single::Exp10(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Exp2` -- `(Half)Single.Exp2((float)...)`. */
        [[nodiscard]] static Half Exp2(Half x) noexcept { return FromSingle(System::Single::Exp2(x.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.Hypot` -- `(Half)Single.Hypot((float)...)`. */
        [[nodiscard]] static Half Hypot(Half x, Half y) noexcept { return FromSingle(System::Single::Hypot(x.ToSingle(), y.ToSingle())); }
        /** @brief C++ counterpart of .NET `Half.RootN` -- `(Half)Single.RootN((float)...)`. */
        [[nodiscard]] static Half RootN(Half x, SharpRuntime::intcs n) noexcept { return FromSingle(System::Single::RootN(x.ToSingle(), n)); }

        // -----------------------------------------------------------------------------------
        // #2384 unit 2a: rounding, Sign, and the IEEE 754:2019 *Number family.
        //
        // The *Number members are NOT the same as Max/Min: they are `maximumNumber` /
        // `minimumNumber`, which DO NOT PROPAGATE NaN and treat +0 as larger than -0. Both
        // differences are real and both are pinned -- a forward to MathF::Max would satisfy every
        // ordinary row and fail exactly those two.
        // -----------------------------------------------------------------------------------

        /** @brief The smallest integral value >= @p x. .NET: `(Half)MathF.Ceiling((float)x)`
         *  (`Half.cs:1312`). */
        [[nodiscard]] static Half Ceiling(Half x) noexcept;
        /** @brief The largest integral value <= @p x. .NET: `Half.cs:1323`. */
        [[nodiscard]] static Half Floor(Half x) noexcept;
        /** @brief Rounds to the nearest integral value, ties to even. .NET: `Half.cs:1326`. */
        [[nodiscard]] static Half Round(Half x) noexcept;
        /** @brief Rounds to @p digits fractional digits. .NET: `Half.cs:1329`. */
        [[nodiscard]] static Half Round(Half x, SharpRuntime::intcs digits);
        /** @brief The integral part of @p x. .NET: `Half.cs:1338`. */
        [[nodiscard]] static Half Truncate(Half x) noexcept;

        /**
         * @brief The sign of @p value: -1, 0 or +1.
         *
         * C++ counterpart of .NET `Half.Sign(Half)` (`Half.cs:1723-1740`).
         * @throws System::ArithmeticException if @p value is NaN -- .NET throws
         *         `ArithmeticException(SR.Arithmetic_NaN)` rather than returning a sentinel, and
         *         that is transcribed rather than softened.
         * @note `Sign(-0.0)` is **0**, not -1: .NET tests `IsZero` BEFORE `IsNegative`, so the
         *       sign of a signed zero is zero.
         */
        [[nodiscard]] static SharpRuntime::intcs Sign(Half value);

        /**
         * @brief IEEE 754:2019 `maximumNumber`. .NET: `Half.cs:1673-1692`.
         * @note **Does not propagate NaN** -- unlike @c Max, a NaN operand is ignored and the
         *       other is returned. And **+0 is treated as larger than -0**, which no comparison
         *       can see, so both are pinned.
         */
        [[nodiscard]] static Half MaxNumber(Half x, Half y) noexcept;
        /** @brief IEEE 754:2019 `minimumNumber`. .NET: `Half.cs:1701-1720`. See @c MaxNumber. */
        [[nodiscard]] static Half MinNumber(Half x, Half y) noexcept;
        /** @brief IEEE 754:2019 `maximumMagnitudeNumber`. .NET: `Half.cs:1854-1876`. */
        [[nodiscard]] static Half MaxMagnitudeNumber(Half x, Half y) noexcept;
        /** @brief IEEE 754:2019 `minimumMagnitudeNumber`. .NET: `Half.cs:1882-1904`. */
        [[nodiscard]] static Half MinMagnitudeNumber(Half x, Half y) noexcept;

        /**
         * @brief The larger of two values by the `>` operator alone.
         *
         * .NET: `(x > y) ? x : y` (`Half.cs:1670`). **`System::Numerics::BFloat16` has NO
         * counterpart**, and that is not an omission here: .NET declares `MaxNative`, `MinNative`,
         * `ClampNative` and `MultiplyAddEstimate` on `Half` ONLY. So #2340's in-step rule means
         * *each type gets what .NET gives it*, not *the two surfaces are identical* -- measured,
         * not assumed.
         */
        [[nodiscard]] static Half MaxNative(Half x, Half y) noexcept { return (x > y) ? x : y; }
        /** @brief The smaller of two values by `<` alone. .NET: `Half.cs:1698`. Half-only. */
        [[nodiscard]] static Half MinNative(Half x, Half y) noexcept { return (x < y) ? x : y; }

        /** @brief Represents the largest finite half-precision value (65504). C++ counterpart of .NET Half.MaxValue. */
        static const Half MaxValue;
        /** @brief Represents the most negative finite half-precision value (-65504). C++ counterpart of .NET Half.MinValue. */
        static const Half MinValue;
        /** @brief Represents the smallest positive half-precision value (~5.96e-8). C++ counterpart of .NET Half.Epsilon. */
        static const Half Epsilon;
        /** @brief Represents the natural logarithmic base, e. C++ counterpart of .NET Half.E. */
        static const Half E;
        /** @brief Represents the ratio of a circle's circumference to its diameter, pi. C++ counterpart of .NET Half.Pi. */
        static const Half Pi;
        /** @brief Represents the number of radians in one turn, tau. C++ counterpart of .NET Half.Tau. */
        static const Half Tau;

        /**
         * @brief Returns true if the value is Not a Number (NaN).
         * C++ counterpart of .NET Half.IsNaN(Half).
         */
        [[nodiscard]] static bool IsNaN(Half h) noexcept {
            return (h.bits & 0x7FFF) > 0x7C00;
        }

        /**
         * @brief Returns true if the value is positive or negative infinity.
         * C++ counterpart of .NET Half.IsInfinity(Half).
         */
        [[nodiscard]] static bool IsInfinity(Half h) noexcept {
            return (h.bits & 0x7FFF) == 0x7C00;
        }

        /**
         * @brief Returns true if the value is positive infinity.
         * C++ counterpart of .NET Half.IsPositiveInfinity(Half).
         */
        [[nodiscard]] static bool IsPositiveInfinity(Half h) noexcept {
            return h.bits == 0x7C00;
        }

        /**
         * @brief Returns true if the value is negative infinity.
         * C++ counterpart of .NET Half.IsNegativeInfinity(Half).
         */
        [[nodiscard]] static bool IsNegativeInfinity(Half h) noexcept {
            return h.bits == 0xFC00;
        }

        /**
         * @brief Returns true if the value is finite (not NaN and not infinity).
         * C++ counterpart of .NET Half.IsFinite(Half).
         */
        [[nodiscard]] static bool IsFinite(Half h) noexcept {
            return (h.bits & 0x7C00) != 0x7C00;
        }

        /**
         * @brief Returns true if the value is negative.
         * C++ counterpart of .NET Half.IsNegative(Half).
         */
        [[nodiscard]] static bool IsNegative(Half h) noexcept {
            return (h.bits & 0x8000) != 0;
        }

        /**
         * @brief Returns true if the value is normal (finite, nonzero, and not subnormal).
         * C++ counterpart of .NET Half.IsNormal(Half).
         */
        [[nodiscard]] static bool IsNormal(Half h) noexcept {
            const uint16_t absBits = h.bits & 0x7FFF;
            return absBits >= 0x0400 && absBits < 0x7C00;
        }

        /**
         * @brief Returns true if the value is subnormal (finite, nonzero, and not normal).
         * C++ counterpart of .NET Half.IsSubnormal(Half).
         */
        [[nodiscard]] static bool IsSubnormal(Half h) noexcept {
            const uint16_t absBits = h.bits & 0x7FFF;
            return absBits >= 1 && absBits <= 0x03FF;
        }

        /**
         * @brief Parses a string into a Half. C++ counterpart of .NET Half.Parse(string).
         * @throws System::FormatException if the string is not a valid floating-point literal.
         */
        [[nodiscard]] static Half Parse(const std::string& s) { return FromSingle(Single::Parse(s)); }

        /** @brief Tries to parse a string into a Half without throwing. C++ counterpart of .NET Half.TryParse. */
        static bool TryParse(const std::string& s, Half& result) noexcept {
            float f;
            if (!Single::TryParse(s, f)) { result = Half(); return false; }
            result = FromSingle(f);
            return true;
        }

        /** @brief C++ counterpart of .NET Half.Parse(string, IFormatProvider). The provider is ignored. */
        [[nodiscard]] static Half Parse(const std::string& s, const IFormatProvider* provider) {
            (void)provider;
            return Parse(s);
        }

        /** @brief C++ counterpart of .NET Half.TryParse(string, IFormatProvider, out Half). The provider is ignored. */
        static bool TryParse(const std::string& s, const IFormatProvider* provider, Half& result) noexcept {
            (void)provider;
            return TryParse(s, result);
        }

        /**
         * @brief Tries to format this Half into the provided character span.
         * C++ counterpart of .NET Half.TryFormat(Span&lt;char&gt;, out int, ReadOnlySpan&lt;char&gt;, IFormatProvider).
         */
        bool TryFormat(Span<char> destination, intcs& charsWritten, const std::string& format = "") const {
            const std::string s = ToString(format);
            if (static_cast<intcs>(s.size()) > destination.getLengthProperty()) {
                charsWritten = 0;
                return false;
            }
            std::copy(s.begin(), s.end(), destination.getPointer());
            charsWritten = static_cast<intcs>(s.size());
            return true;
        }

        /**
         * @brief Returns a string representation of this Half value.
         * C++ counterpart of .NET Half.ToString(). Uses up to 5 significant digits, which
         * is always sufficient to round-trip any binary16 value (a documented simplification
         * of .NET's shortest-round-trip formatter; see class-level note).
         */
        [[nodiscard]] std::string ToString() const {
            if (IsNaN(*this)) return "NaN";
            if (bits == 0x7C00) return "Infinity";
            if (bits == 0xFC00) return "-Infinity";
            std::ostringstream oss;
            oss.imbue(std::locale::classic());
            oss << std::setprecision(5) << ToDouble();
            return oss.str();
        }

        /** @brief Returns a string representation using the specified numeric format ("F2", "E3", "G", "R"). */
        [[nodiscard]] std::string ToString(const std::string& format) const {
            if (format.empty()) return ToString();
            if (IsNaN(*this)) return "NaN";
            if (bits == 0x7C00) return "Infinity";
            if (bits == 0xFC00) return "-Infinity";
            return Single::ToString(ToSingle(), format);
        }

        /** @brief C++ counterpart of .NET Half.ToString(IFormatProvider). The provider is ignored. */
        [[nodiscard]] std::string ToString(const IFormatProvider* provider) const { (void)provider; return ToString(); }

        /** @brief C++ counterpart of .NET Half.ToString(string, IFormatProvider). The provider is ignored. */
        [[nodiscard]] std::string ToString(const std::string& format, const IFormatProvider* provider) const {
            (void)provider;
            return ToString(format);
        }

        /**
         * @brief Returns a hash code for this Half value.
         * C++ counterpart of .NET Half.GetHashCode(): all NaNs and both zeros hash the same.
         */
        [[nodiscard]] intcs GetHashCode() const noexcept {
            // Real Half.GetHashCode() does `bits &= PositiveInfinityBits` (an AND-mask, not an
            // assignment): for NaN this is a no-op in effect (the exponent field is already
            // all-ones, so it survives the 0x7C00 mask unchanged, giving 0x7C00), but for zero
            // (+0 or -0, exponent field 0) it masks everything away to 0 -- NOT 0x7C00.
            // Assigning b=0x7C00 unconditionally for both cases (the previous code here) still
            // satisfied the hash contract (equal values -- verified via Equals() above, which
            // does treat +0/-0 and all NaNs as mutually equal -- still hashed equally), but
            // didn't match .NET's actual hash value for zero.
            uint16_t b = bits;
            if (IsNaN(*this) || (bits & 0x7FFF) == 0) b &= 0x7C00;
            return static_cast<intcs>(b);
        }

        /**
         * @brief Determines whether this instance equals @p other.
         * C++ counterpart of .NET Half.Equals(Half): unlike operator==, NaN equals NaN here.
         */
        [[nodiscard]] bool Equals(const Half& other) const noexcept {
            return bits == other.bits
                || (((bits | other.bits) & 0x7FFF) == 0)
                || (IsNaN(*this) && IsNaN(other));
        }

        /**
         * @brief Compares this Half to another.
         * C++ counterpart of .NET Half.CompareTo(Half).
         * @return negative if less, 0 if equal, positive if greater; NaN sorts less than
         *         everything except another NaN (which it's equal to) — matching .NET's
         *         Half/Single/Double.CompareTo convention (NaN sorts first, ascending).
         */
        [[nodiscard]] intcs CompareTo(const Half& other) const noexcept {
            if (*this < other) return -1;
            if (*this > other) return 1;
            if (*this == other) return 0;
            if (IsNaN(*this)) return IsNaN(other) ? 0 : -1;
            return 1;
        }

        /** @brief Returns true if this Half is equal to @p o (IEEE 754 semantics: NaN != NaN, +0 == -0). */
        bool operator==(const Half& o) const noexcept { return ToSingle() == o.ToSingle(); }
        /** @brief Returns true if this Half is not equal to @p o. */
        bool operator!=(const Half& o) const noexcept { return !(*this == o); }
        /** @brief Returns true if this Half is less than @p o. */
        bool operator< (const Half& o) const noexcept { return ToSingle() <  o.ToSingle(); }
        /** @brief Returns true if this Half is less than or equal to @p o. */
        bool operator<=(const Half& o) const noexcept { return ToSingle() <= o.ToSingle(); }
        /** @brief Returns true if this Half is greater than @p o. */
        bool operator> (const Half& o) const noexcept { return ToSingle() >  o.ToSingle(); }
        /** @brief Returns true if this Half is greater than or equal to @p o. */
        bool operator>=(const Half& o) const noexcept { return ToSingle() >= o.ToSingle(); }

        /** @brief Adds two Half values. C++ counterpart of .NET Half.operator+(Half,Half). */
        friend Half operator+(const Half& l, const Half& r) noexcept { return FromSingle(l.ToSingle() + r.ToSingle()); }
        /** @brief Subtracts two Half values. C++ counterpart of .NET Half.operator-(Half,Half). */
        friend Half operator-(const Half& l, const Half& r) noexcept { return FromSingle(l.ToSingle() - r.ToSingle()); }
        /** @brief Multiplies two Half values. C++ counterpart of .NET Half.operator*(Half,Half). */
        friend Half operator*(const Half& l, const Half& r) noexcept { return FromSingle(l.ToSingle() * r.ToSingle()); }
        /** @brief Divides two Half values. C++ counterpart of .NET Half.operator/(Half,Half). */
        friend Half operator/(const Half& l, const Half& r) noexcept { return FromSingle(l.ToSingle() / r.ToSingle()); }
        /** @brief Computes the remainder of two Half values. C++ counterpart of .NET Half.operator%(Half,Half). */
        friend Half operator%(const Half& l, const Half& r) noexcept { return FromSingle(std::fmod(l.ToSingle(), r.ToSingle())); }

        /** @brief Unary plus. C++ counterpart of .NET Half.operator+(Half). */
        Half operator+() const noexcept { return *this; }
        /** @brief Unary negation. C++ counterpart of .NET Half.operator-(Half). */
        Half operator-() const noexcept { return Half(static_cast<uint16_t>(bits ^ 0x8000u)); }

        /** @brief Pre-increment by one. C++ counterpart of .NET Half.operator++(Half). */
        Half& operator++() noexcept { *this = FromSingle(ToSingle() + 1.0f); return *this; }
        /** @brief Post-increment by one. */
        Half operator++(int) noexcept { Half tmp = *this; ++(*this); return tmp; }
        /** @brief Pre-decrement by one. C++ counterpart of .NET Half.operator--(Half). */
        Half& operator--() noexcept { *this = FromSingle(ToSingle() - 1.0f); return *this; }
        /** @brief Post-decrement by one. */
        Half operator--(int) noexcept { Half tmp = *this; --(*this); return tmp; }
    };

    inline const Half Half::Zero             = Half(0x0000);
    inline const Half Half::One              = Half(0x3C00);
    inline const Half Half::NegativeOne      = Half(0xBC00);
    inline const Half Half::NegativeZero     = Half(0x8000);
    inline const Half Half::NaN              = Half(0xFE00);
    inline const Half Half::PositiveInfinity = Half(0x7C00);
    inline const Half Half::NegativeInfinity = Half(0xFC00);
    // #2384 unit 2a definitions.
    inline Half Half::Ceiling(Half x) noexcept  { return FromSingle(System::MathF::Ceiling(x.ToSingle())); }
    inline Half Half::Floor(Half x) noexcept    { return FromSingle(System::MathF::Floor(x.ToSingle())); }
    inline Half Half::Round(Half x) noexcept    { return FromSingle(System::MathF::Round(x.ToSingle())); }
    inline Half Half::Round(Half x, SharpRuntime::intcs digits) {
        return FromSingle(System::MathF::Round(x.ToSingle(), digits));
    }
    inline Half Half::Truncate(Half x) noexcept { return FromSingle(System::MathF::Truncate(x.ToSingle())); }

    inline SharpRuntime::intcs Half::Sign(Half value) {
        // .NET throws rather than returning a sentinel, and tests IsZero BEFORE IsNegative, so
        // Sign(-0.0) is 0 rather than -1.
        if (IsNaN(value)) throw System::ArithmeticException("Function does not accept floating point Not-a-Number values.");
        if ((value.bits & 0x7FFFu) == 0u) return 0;
        return IsNegative(value) ? -1 : 1;
    }

    inline Half Half::MaxNumber(Half x, Half y) noexcept {
        if (!(x.ToSingle() == y.ToSingle())) {          // x != y, with NaN making this true
            if (!IsNaN(y)) return (y.ToSingle() < x.ToSingle()) ? x : y;
            return x;
        }
        return IsNegative(y) ? x : y;                   // equal: +0 is larger than -0
    }
    inline Half Half::MinNumber(Half x, Half y) noexcept {
        if (!(x.ToSingle() == y.ToSingle())) {
            if (!IsNaN(y)) return (x.ToSingle() < y.ToSingle()) ? x : y;
            return x;
        }
        return IsNegative(x) ? x : y;
    }
    inline Half Half::MaxMagnitudeNumber(Half x, Half y) noexcept {
        const float ax = Abs(x).ToSingle();
        const float ay = Abs(y).ToSingle();
        if ((ax > ay) || IsNaN(y)) return x;
        if (ax == ay) return IsNegative(x) ? y : x;
        return y;
    }
    inline Half Half::MinMagnitudeNumber(Half x, Half y) noexcept {
        const float ax = Abs(x).ToSingle();
        const float ay = Abs(y).ToSingle();
        if ((ax < ay) || IsNaN(y)) return x;
        if (ax == ay) return IsNegative(x) ? x : y;
        return y;
    }

    // #2384 unit 1: the four float round-trip members, defined after the constants they need.
    // Each is .NET's own expression, not a re-derivation -- Clamp is float.Clamp, Max/Min are
    // float.Max/float.Min, and MaxMagnitude/MinMagnitude are MathF.MaxMagnitude/MinMagnitude.
    inline Half Half::Clamp(Half value, Half min, Half max) {
        return FromSingle(System::MathF::Clamp(value.ToSingle(), min.ToSingle(), max.ToSingle()));
    }
    inline Half Half::Max(Half x, Half y) noexcept {
        return FromSingle(System::MathF::Max(x.ToSingle(), y.ToSingle()));
    }
    inline Half Half::Min(Half x, Half y) noexcept {
        return FromSingle(System::MathF::Min(x.ToSingle(), y.ToSingle()));
    }
    inline Half Half::MaxMagnitude(Half x, Half y) noexcept {
        return FromSingle(System::MathF::MaxMagnitude(x.ToSingle(), y.ToSingle()));
    }
    inline Half Half::MinMagnitude(Half x, Half y) noexcept {
        return FromSingle(System::MathF::MinMagnitude(x.ToSingle(), y.ToSingle()));
    }

    inline const Half Half::MaxValue         = Half(0x7BFF);  //  65504
    inline const Half Half::MinValue         = Half(0xFBFF);  // -65504
    inline const Half Half::Epsilon          = Half(0x0001);  //  ~5.96e-8
    inline const Half Half::E                = Half(0x4170);  //  ~2.71875
    inline const Half Half::Pi               = Half(0x4248);  //  ~3.140625
    inline const Half Half::Tau              = Half(0x4648);  //  ~6.28125

} // namespace System
