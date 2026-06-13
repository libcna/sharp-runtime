// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>

#if defined(_MSC_VER)
#  error "Int128 requires __int128 (GCC/Clang only). MSVC is not supported for this type."
#endif

namespace System {

    /// 128-bit signed integer, mirroring .NET System.Int128 (GCC/Clang only).
    class Int128 {
        __int128 value_;

    public:
        /// Constructs an Int128 with value 0.
        constexpr Int128() : value_(0) {}
        /// Constructs an Int128 from a raw __int128 value.
        constexpr explicit Int128(__int128 v) : value_(v) {}
        /// @brief Constructs an Int128 from a 64-bit upper half and a 64-bit lower half.
        /// @param upper Signed high 64 bits.
        /// @param lower Unsigned low 64 bits.
        constexpr Int128(int64_t upper, uint64_t lower)
            : value_((static_cast<__int128>(upper) << 64) | lower) {}

        /// Returns the minimum representable Int128 value.
        static const Int128 MinValue() {
            static Int128 v(static_cast<__int128>(static_cast<int64_t>(0x8000000000000000LL)) << 64);
            return v;
        }
        /// Returns the maximum representable Int128 value.
        static const Int128 MaxValue() {
            static Int128 v((static_cast<__int128>(static_cast<int64_t>(0x7FFFFFFFFFFFFFFFLL)) << 64) | 0xFFFFFFFFFFFFFFFFULL);
            return v;
        }
        /// Returns an Int128 with value 0.
        static const Int128 Zero() { return Int128(0); }
        /// Returns an Int128 with value 1.
        static const Int128 One()  { return Int128(1); }

        /// Returns the low 64 bits as an unsigned 64-bit integer.
        [[nodiscard]] uint64_t getLowerProperty() const { return static_cast<uint64_t>(value_); }
        /// Returns the high 64 bits as a signed 64-bit integer.
        [[nodiscard]] int64_t  getUpperProperty() const { return static_cast<int64_t>(value_ >> 64); }

        /// Explicit conversion to long long (truncates to 64 bits).
        explicit operator long long()          const { return static_cast<long long>(value_); }
        /// Explicit conversion to unsigned long long (truncates to 64 bits).
        explicit operator unsigned long long() const { return static_cast<unsigned long long>(value_); }
        /// Explicit conversion to double (may lose precision).
        explicit operator double()             const { return static_cast<double>(value_); }
        /// Explicit conversion to __int128.
        explicit operator __int128()           const { return value_; }

        /// Addition operator.
        Int128 operator+(const Int128& o) const { return Int128(value_ + o.value_); }
        /// Subtraction operator.
        Int128 operator-(const Int128& o) const { return Int128(value_ - o.value_); }
        /// Multiplication operator.
        Int128 operator*(const Int128& o) const { return Int128(value_ * o.value_); }
        /// Division operator.
        Int128 operator/(const Int128& o) const { return Int128(value_ / o.value_); }
        /// Modulo operator.
        Int128 operator%(const Int128& o) const { return Int128(value_ % o.value_); }
        /// Bitwise AND operator.
        Int128 operator&(const Int128& o) const { return Int128(value_ & o.value_); }
        /// Bitwise OR operator.
        Int128 operator|(const Int128& o) const { return Int128(value_ | o.value_); }
        /// Bitwise XOR operator.
        Int128 operator^(const Int128& o) const { return Int128(value_ ^ o.value_); }
        /// Bitwise NOT operator.
        Int128 operator~()               const { return Int128(~value_); }
        /// Unary negation operator.
        Int128 operator-()               const { return Int128(-value_); }
        /// Left-shift operator.
        Int128 operator<<(int n)         const { return Int128(value_ << n); }
        /// Right-shift operator.
        Int128 operator>>(int n)         const { return Int128(value_ >> n); }

        /// Equality comparison.
        bool operator==(const Int128& o) const { return value_ == o.value_; }
        /// Inequality comparison.
        bool operator!=(const Int128& o) const { return value_ != o.value_; }
        /// Less-than comparison.
        bool operator< (const Int128& o) const { return value_ <  o.value_; }
        /// Less-than-or-equal comparison.
        bool operator<=(const Int128& o) const { return value_ <= o.value_; }
        /// Greater-than comparison.
        bool operator> (const Int128& o) const { return value_ >  o.value_; }
        /// Greater-than-or-equal comparison.
        bool operator>=(const Int128& o) const { return value_ >= o.value_; }

        /// Returns the decimal string representation of this Int128.
        [[nodiscard]] std::string ToString() const {
            if (value_ == 0) return "0";
            bool neg = value_ < 0;
            __int128 v = neg ? -value_ : value_;
            std::string s;
            while (v > 0) { s += char('0' + (int)(v % 10)); v /= 10; }
            if (neg) s += '-';
            std::reverse(s.begin(), s.end());
            return s;
        }
    };

} // namespace System
