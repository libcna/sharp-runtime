// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <cstring>
#include <string>

namespace System::Numerics {

    // Brain float 16: 1 sign bit, 8 exponent bits, 7 mantissa bits — same as top 16 bits of float32.
    class BFloat16 {
        uint16_t bits_;

        static uint16_t fromFloat(float f) {
            uint32_t u;
            std::memcpy(&u, &f, 4);
            return static_cast<uint16_t>(u >> 16);
        }
        static float toFloat(uint16_t bits) {
            uint32_t u = static_cast<uint32_t>(bits) << 16;
            float f;
            std::memcpy(&f, &u, 4);
            return f;
        }

    public:
        constexpr BFloat16() : bits_(0) {}
        constexpr explicit BFloat16(uint16_t rawBits) : bits_(rawBits) {}
        explicit BFloat16(float v) : bits_(fromFloat(v)) {}

        static BFloat16 Zero()        { return BFloat16(0); }
        static BFloat16 One()         { return BFloat16(0x3F80u); }
        static BFloat16 NegativeOne() { return BFloat16(0xBF80u); }
        static BFloat16 MaxValue()    { return BFloat16(0x7F7Fu); }
        static BFloat16 MinValue()    { return BFloat16(0xFF7Fu); }
        static BFloat16 Epsilon()     { return BFloat16(0x0001u); }
        static BFloat16 NaN()         { return BFloat16(0x7FC0u); }
        static BFloat16 PositiveInfinity() { return BFloat16(0x7F80u); }
        static BFloat16 NegativeInfinity() { return BFloat16(0xFF80u); }

        [[nodiscard]] uint16_t getBitsProperty() const { return bits_; }

        explicit operator float()  const { return toFloat(bits_); }
        explicit operator double() const { return static_cast<double>(toFloat(bits_)); }

        BFloat16 operator+(const BFloat16& o) const { return BFloat16(toFloat(bits_) + toFloat(o.bits_)); }
        BFloat16 operator-(const BFloat16& o) const { return BFloat16(toFloat(bits_) - toFloat(o.bits_)); }
        BFloat16 operator*(const BFloat16& o) const { return BFloat16(toFloat(bits_) * toFloat(o.bits_)); }
        BFloat16 operator/(const BFloat16& o) const { return BFloat16(toFloat(bits_) / toFloat(o.bits_)); }
        BFloat16 operator-()                  const { return BFloat16(static_cast<uint16_t>(bits_ ^ 0x8000u)); }

        bool operator==(const BFloat16& o) const { return bits_ == o.bits_; }
        bool operator!=(const BFloat16& o) const { return bits_ != o.bits_; }
        bool operator< (const BFloat16& o) const { return toFloat(bits_) <  toFloat(o.bits_); }
        bool operator<=(const BFloat16& o) const { return toFloat(bits_) <= toFloat(o.bits_); }
        bool operator> (const BFloat16& o) const { return toFloat(bits_) >  toFloat(o.bits_); }
        bool operator>=(const BFloat16& o) const { return toFloat(bits_) >= toFloat(o.bits_); }

        static bool IsNaN(BFloat16 v)              { return (v.bits_ & 0x7FFFu) > 0x7F80u; }
        static bool IsInfinity(BFloat16 v)         { return (v.bits_ & 0x7FFFu) == 0x7F80u; }
        static bool IsPositiveInfinity(BFloat16 v) { return v.bits_ == 0x7F80u; }
        static bool IsNegativeInfinity(BFloat16 v) { return v.bits_ == 0xFF80u; }

        [[nodiscard]] std::string ToString() const { return std::to_string(toFloat(bits_)); }
    };

} // namespace System::Numerics
