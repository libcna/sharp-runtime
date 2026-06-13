// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>

#if defined(_MSC_VER)
#  error "UInt128 requires unsigned __int128 (GCC/Clang only). MSVC is not supported for this type."
#endif

namespace System {

    /// 128-bit unsigned integer, mirroring .NET System.UInt128 (GCC/Clang only).
    class UInt128 {
        unsigned __int128 value_;

    public:
        /// Constructs a UInt128 with value 0.
        constexpr UInt128() : value_(0) {}
        /// Constructs a UInt128 from a raw unsigned __int128 value.
        constexpr explicit UInt128(unsigned __int128 v) : value_(v) {}
        /// @brief Constructs a UInt128 from a 64-bit upper half and a 64-bit lower half.
        /// @param upper High 64 bits.
        /// @param lower Low 64 bits.
        constexpr UInt128(uint64_t upper, uint64_t lower)
            : value_((static_cast<unsigned __int128>(upper) << 64) | lower) {}

        /// Returns the minimum representable UInt128 value (0).
        static const UInt128 MinValue() { return UInt128(static_cast<unsigned __int128>(0)); }
        /// Returns the maximum representable UInt128 value.
        static const UInt128 MaxValue() {
            return UInt128((static_cast<unsigned __int128>(0xFFFFFFFFFFFFFFFFULL) << 64) | 0xFFFFFFFFFFFFFFFFULL);
        }
        /// Returns a UInt128 with value 0.
        static const UInt128 Zero() { return UInt128(0); }
        /// Returns a UInt128 with value 1.
        static const UInt128 One()  { return UInt128(1); }

        /// Returns the low 64 bits as an unsigned 64-bit integer.
        [[nodiscard]] uint64_t getLowerProperty() const { return static_cast<uint64_t>(value_); }
        /// Returns the high 64 bits as an unsigned 64-bit integer.
        [[nodiscard]] uint64_t getUpperProperty() const { return static_cast<uint64_t>(value_ >> 64); }

        /// Explicit conversion to unsigned long long (truncates to 64 bits).
        explicit operator unsigned long long() const { return static_cast<unsigned long long>(value_); }
        /// Explicit conversion to long long (truncates to 64 bits, may change sign).
        explicit operator long long()          const { return static_cast<long long>(value_); }
        /// Explicit conversion to double (may lose precision).
        explicit operator double()             const { return static_cast<double>(value_); }
        /// Explicit conversion to unsigned __int128.
        explicit operator unsigned __int128()  const { return value_; }

        /// Addition operator.
        UInt128 operator+(const UInt128& o) const { return UInt128(value_ + o.value_); }
        /// Subtraction operator.
        UInt128 operator-(const UInt128& o) const { return UInt128(value_ - o.value_); }
        /// Multiplication operator.
        UInt128 operator*(const UInt128& o) const { return UInt128(value_ * o.value_); }
        /// Division operator.
        UInt128 operator/(const UInt128& o) const { return UInt128(value_ / o.value_); }
        /// Modulo operator.
        UInt128 operator%(const UInt128& o) const { return UInt128(value_ % o.value_); }
        /// Bitwise AND operator.
        UInt128 operator&(const UInt128& o) const { return UInt128(value_ & o.value_); }
        /// Bitwise OR operator.
        UInt128 operator|(const UInt128& o) const { return UInt128(value_ | o.value_); }
        /// Bitwise XOR operator.
        UInt128 operator^(const UInt128& o) const { return UInt128(value_ ^ o.value_); }
        /// Bitwise NOT operator.
        UInt128 operator~()               const { return UInt128(~value_); }
        /// Left-shift operator.
        UInt128 operator<<(int n)         const { return UInt128(value_ << n); }
        /// Right-shift operator.
        UInt128 operator>>(int n)         const { return UInt128(value_ >> n); }

        /// Equality comparison.
        bool operator==(const UInt128& o) const { return value_ == o.value_; }
        /// Inequality comparison.
        bool operator!=(const UInt128& o) const { return value_ != o.value_; }
        /// Less-than comparison.
        bool operator< (const UInt128& o) const { return value_ <  o.value_; }
        /// Less-than-or-equal comparison.
        bool operator<=(const UInt128& o) const { return value_ <= o.value_; }
        /// Greater-than comparison.
        bool operator> (const UInt128& o) const { return value_ >  o.value_; }
        /// Greater-than-or-equal comparison.
        bool operator>=(const UInt128& o) const { return value_ >= o.value_; }

        /// Returns the decimal string representation of this UInt128.
        [[nodiscard]] std::string ToString() const {
            if (value_ == 0) return "0";
            unsigned __int128 v = value_;
            std::string s;
            while (v > 0) { s += char('0' + (int)(v % 10)); v /= 10; }
            std::reverse(s.begin(), s.end());
            return s;
        }
    };

} // namespace System
