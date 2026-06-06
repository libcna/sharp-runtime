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
        uint16_t bits = 0;

        Half() = default;
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

        explicit operator float() const { return ToSingle(); }

        static const Half Zero;
        static const Half NaN;
        static const Half PositiveInfinity;
        static const Half NegativeInfinity;
        static const Half MaxValue;
        static const Half MinValue;
        static const Half Epsilon;

        bool operator==(const Half& o) const { return bits == o.bits; }
        bool operator!=(const Half& o) const { return bits != o.bits; }
        bool operator< (const Half& o) const { return ToSingle() <  o.ToSingle(); }
        bool operator<=(const Half& o) const { return ToSingle() <= o.ToSingle(); }
        bool operator> (const Half& o) const { return ToSingle() >  o.ToSingle(); }
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
