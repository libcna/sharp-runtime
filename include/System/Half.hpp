// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <cstring>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    /**
     * @brief Represents a half-precision (16-bit) floating-point number.
     *
     * Partial C++ counterpart of .NET System.Half.
     * Conversion uses standard IEEE 754 half-precision bit manipulation.
     *
     * @note Status: Partial — arithmetic not overloaded; use ToSingle/FromSingle for computation.
     */
    struct Half {
        /// The raw IEEE 754 half-precision bit pattern.
        uint16_t bits = 0;

        /// Initializes a new Half with zero value.
        Half() = default;
        /// Initializes a new Half from a raw 16-bit bit pattern.
        explicit Half(uint16_t rawBits) : bits(rawBits) {}

        /** @brief Converts a 32-bit float to Half. */
        static Half FromSingle(float value) {
            uint32_t f;
            std::memcpy(&f, &value, 4);
            uint32_t sign  = (f >> 31) & 0x1;
            int32_t  exp   = static_cast<int32_t>((f >> 23) & 0xFF) - 127;
            uint32_t mant  = f & 0x7FFFFF;

            uint16_t h;
            if (exp >= 16)       { h = static_cast<uint16_t>((sign << 15) | 0x7C00); } // inf/overflow
            else if (exp >= -14) { h = static_cast<uint16_t>((sign << 15) | ((exp + 15) << 10) | (mant >> 13)); }
            else if (exp >= -24) { h = static_cast<uint16_t>((sign << 15) | ((mant | 0x800000) >> (-exp - 2))); }
            else                 { h = static_cast<uint16_t>(sign << 15); } // underflow → ±0
            Half result; result.bits = h; return result;
        }

        /** @brief Converts this Half to a 32-bit float. */
        [[nodiscard]] float ToSingle() const {
            uint32_t sign = (bits >> 15) & 0x1;
            uint32_t exp  = (bits >> 10) & 0x1F;
            uint32_t mant = bits & 0x3FF;
            uint32_t f;
            if      (exp == 0x1F) { f = (sign << 31) | 0x7F800000 | (mant << 13); } // inf/NaN
            else if (exp == 0)    { f = (sign << 31) | (mant << 13); }               // denorm
            else                  { f = (sign << 31) | ((exp + 112) << 23) | (mant << 13); }
            float result;
            std::memcpy(&result, &f, 4);
            return result;
        }

        /// Explicit conversion to a 32-bit float.
        explicit operator float() const { return ToSingle(); }

        /// Represents the value zero.
        static const Half Zero;
        /// Represents Not a Number (NaN).
        static const Half NaN;
        /// Represents positive infinity.
        static const Half PositiveInfinity;
        /// Represents negative infinity.
        static const Half NegativeInfinity;
        /// Represents the largest finite half-precision value (65504).
        static const Half MaxValue;
        /// Represents the most negative finite half-precision value (-65504).
        static const Half MinValue;
        /// Represents the smallest positive half-precision value (~5.96e-8).
        static const Half Epsilon;

        /// Returns true if this Half is equal to the specified Half.
        bool operator==(const Half& o) const { return bits == o.bits; }
        /// Returns true if this Half is not equal to the specified Half.
        bool operator!=(const Half& o) const { return bits != o.bits; }
        /// Returns true if this Half is less than the specified Half.
        bool operator< (const Half& o) const { return ToSingle() <  o.ToSingle(); }
        /// Returns true if this Half is less than or equal to the specified Half.
        bool operator<=(const Half& o) const { return ToSingle() <= o.ToSingle(); }
        /// Returns true if this Half is greater than the specified Half.
        bool operator> (const Half& o) const { return ToSingle() >  o.ToSingle(); }
        /// Returns true if this Half is greater than or equal to the specified Half.
        bool operator>=(const Half& o) const { return ToSingle() >= o.ToSingle(); }
    };

    inline const Half Half::Zero             = Half::FromSingle(0.0f);
    inline const Half Half::NaN              = Half(0x7E00);
    inline const Half Half::PositiveInfinity = Half(0x7C00);
    inline const Half Half::NegativeInfinity = Half(0xFC00);
    inline const Half Half::MaxValue         = Half(0x7BFF);  //  65504
    inline const Half Half::MinValue         = Half(0xFBFF);  // -65504
    inline const Half Half::Epsilon          = Half(0x0001);  //  ~5.96e-8

} // namespace System
