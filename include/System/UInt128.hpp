// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>

namespace System {

    class UInt128 {
        unsigned __int128 value_;

    public:
        constexpr UInt128() : value_(0) {}
        constexpr explicit UInt128(unsigned __int128 v) : value_(v) {}
        constexpr UInt128(uint64_t upper, uint64_t lower)
            : value_((static_cast<unsigned __int128>(upper) << 64) | lower) {}

        static const UInt128 MinValue() { return UInt128(static_cast<unsigned __int128>(0)); }
        static const UInt128 MaxValue() {
            return UInt128((static_cast<unsigned __int128>(0xFFFFFFFFFFFFFFFFULL) << 64) | 0xFFFFFFFFFFFFFFFFULL);
        }
        static const UInt128 Zero() { return UInt128(0); }
        static const UInt128 One()  { return UInt128(1); }

        [[nodiscard]] uint64_t getLowerProperty() const { return static_cast<uint64_t>(value_); }
        [[nodiscard]] uint64_t getUpperProperty() const { return static_cast<uint64_t>(value_ >> 64); }

        explicit operator unsigned long long() const { return static_cast<unsigned long long>(value_); }
        explicit operator long long()          const { return static_cast<long long>(value_); }
        explicit operator double()             const { return static_cast<double>(value_); }
        explicit operator unsigned __int128()  const { return value_; }

        UInt128 operator+(const UInt128& o) const { return UInt128(value_ + o.value_); }
        UInt128 operator-(const UInt128& o) const { return UInt128(value_ - o.value_); }
        UInt128 operator*(const UInt128& o) const { return UInt128(value_ * o.value_); }
        UInt128 operator/(const UInt128& o) const { return UInt128(value_ / o.value_); }
        UInt128 operator%(const UInt128& o) const { return UInt128(value_ % o.value_); }
        UInt128 operator&(const UInt128& o) const { return UInt128(value_ & o.value_); }
        UInt128 operator|(const UInt128& o) const { return UInt128(value_ | o.value_); }
        UInt128 operator^(const UInt128& o) const { return UInt128(value_ ^ o.value_); }
        UInt128 operator~()               const { return UInt128(~value_); }
        UInt128 operator<<(int n)         const { return UInt128(value_ << n); }
        UInt128 operator>>(int n)         const { return UInt128(value_ >> n); }

        bool operator==(const UInt128& o) const { return value_ == o.value_; }
        bool operator!=(const UInt128& o) const { return value_ != o.value_; }
        bool operator< (const UInt128& o) const { return value_ <  o.value_; }
        bool operator<=(const UInt128& o) const { return value_ <= o.value_; }
        bool operator> (const UInt128& o) const { return value_ >  o.value_; }
        bool operator>=(const UInt128& o) const { return value_ >= o.value_; }

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
