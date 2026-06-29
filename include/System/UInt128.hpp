// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

#if defined(_MSC_VER)
#  error "UInt128 requires unsigned __int128 (GCC/Clang only). MSVC is not supported for this type."
#endif

namespace System {

    /**
     * @brief 128-bit unsigned integer, mirroring .NET System.UInt128 (GCC/Clang only).
     *
     * C++ counterpart of .NET System.UInt128.
     * Arithmetic and bitwise operators, conversions, and ToString are provided.
     */
    class UInt128 {
        unsigned __int128 value_;

    public:
        /** @brief Constructs a UInt128 with value 0. */
        constexpr UInt128() : value_(0) {}
        /** @brief Constructs a UInt128 from a raw unsigned __int128 value. */
        constexpr explicit UInt128(unsigned __int128 v) : value_(v) {}
        /**
         * @brief Constructs a UInt128 from a 64-bit upper half and a 64-bit lower half.
         * @param upper High 64 bits.
         * @param lower Low 64 bits.
         */
        constexpr UInt128(uint64_t upper, uint64_t lower)
            : value_((static_cast<unsigned __int128>(upper) << 64) | lower) {}

        /** @brief Returns the minimum representable UInt128 value (0). */
        static const UInt128 MinValue() { return UInt128(static_cast<unsigned __int128>(0)); }
        /** @brief Returns the maximum representable UInt128 value. */
        static const UInt128 MaxValue() {
            return UInt128((static_cast<unsigned __int128>(0xFFFFFFFFFFFFFFFFULL) << 64) | 0xFFFFFFFFFFFFFFFFULL);
        }
        /** @brief Returns a UInt128 with value 0. */
        static const UInt128 Zero() { return UInt128(0); }
        /** @brief Returns a UInt128 with value 1. */
        static const UInt128 One()  { return UInt128(1); }

        /** @brief Returns the low 64 bits as an unsigned 64-bit integer. */
        [[nodiscard]] uint64_t getLowerProperty() const { return static_cast<uint64_t>(value_); }
        /** @brief Returns the high 64 bits as an unsigned 64-bit integer. */
        [[nodiscard]] uint64_t getUpperProperty() const { return static_cast<uint64_t>(value_ >> 64); }

        /** @brief Explicit conversion to unsigned long long (truncates to 64 bits). */
        explicit operator unsigned long long() const { return static_cast<unsigned long long>(value_); }
        /** @brief Explicit conversion to long long (truncates to 64 bits, may change sign). */
        explicit operator long long()          const { return static_cast<long long>(value_); }
        /** @brief Explicit conversion to double (may lose precision). */
        explicit operator double()             const { return static_cast<double>(value_); }
        /** @brief Explicit conversion to unsigned __int128. */
        explicit operator unsigned __int128()  const { return value_; }

        /** @brief Addition operator. */
        UInt128 operator+(const UInt128& o) const { return UInt128(value_ + o.value_); }
        /** @brief Subtraction operator. */
        UInt128 operator-(const UInt128& o) const { return UInt128(value_ - o.value_); }
        /** @brief Multiplication operator. */
        UInt128 operator*(const UInt128& o) const { return UInt128(value_ * o.value_); }
        /** @brief Division operator. */
        UInt128 operator/(const UInt128& o) const { return UInt128(value_ / o.value_); }
        /** @brief Modulo operator. */
        UInt128 operator%(const UInt128& o) const { return UInt128(value_ % o.value_); }
        /** @brief Bitwise AND operator. */
        UInt128 operator&(const UInt128& o) const { return UInt128(value_ & o.value_); }
        /** @brief Bitwise OR operator. */
        UInt128 operator|(const UInt128& o) const { return UInt128(value_ | o.value_); }
        /** @brief Bitwise XOR operator. */
        UInt128 operator^(const UInt128& o) const { return UInt128(value_ ^ o.value_); }
        /** @brief Bitwise NOT operator. */
        UInt128 operator~()               const { return UInt128(~value_); }
        /** @brief Left-shift operator. */
        UInt128 operator<<(int n)         const { return UInt128(value_ << n); }
        /** @brief Right-shift operator. */
        UInt128 operator>>(int n)         const { return UInt128(value_ >> n); }

        /** @brief Equality comparison. */
        bool operator==(const UInt128& o) const { return value_ == o.value_; }
        /** @brief Inequality comparison. */
        bool operator!=(const UInt128& o) const { return value_ != o.value_; }
        /** @brief Less-than comparison. */
        bool operator< (const UInt128& o) const { return value_ <  o.value_; }
        /** @brief Less-than-or-equal comparison. */
        bool operator<=(const UInt128& o) const { return value_ <= o.value_; }
        /** @brief Greater-than comparison. */
        bool operator> (const UInt128& o) const { return value_ >  o.value_; }
        /** @brief Greater-than-or-equal comparison. */
        bool operator>=(const UInt128& o) const { return value_ >= o.value_; }

        /** @brief Returns the decimal string representation of this UInt128. */
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
