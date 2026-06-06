// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>

namespace System {

    class Int128 {
        __int128 value_;

    public:
        constexpr Int128() : value_(0) {}
        constexpr explicit Int128(__int128 v) : value_(v) {}
        constexpr Int128(int64_t upper, uint64_t lower)
            : value_((static_cast<__int128>(upper) << 64) | lower) {}

        static const Int128 MinValue() {
            static Int128 v(static_cast<__int128>(static_cast<int64_t>(0x8000000000000000LL)) << 64);
            return v;
        }
        static const Int128 MaxValue() {
            static Int128 v((static_cast<__int128>(static_cast<int64_t>(0x7FFFFFFFFFFFFFFFLL)) << 64) | 0xFFFFFFFFFFFFFFFFULL);
            return v;
        }
        static const Int128 Zero() { return Int128(0); }
        static const Int128 One()  { return Int128(1); }

        [[nodiscard]] uint64_t getLowerProperty() const { return static_cast<uint64_t>(value_); }
        [[nodiscard]] int64_t  getUpperProperty() const { return static_cast<int64_t>(value_ >> 64); }

        explicit operator long long()          const { return static_cast<long long>(value_); }
        explicit operator unsigned long long() const { return static_cast<unsigned long long>(value_); }
        explicit operator double()             const { return static_cast<double>(value_); }
        explicit operator __int128()           const { return value_; }

        Int128 operator+(const Int128& o) const { return Int128(value_ + o.value_); }
        Int128 operator-(const Int128& o) const { return Int128(value_ - o.value_); }
        Int128 operator*(const Int128& o) const { return Int128(value_ * o.value_); }
        Int128 operator/(const Int128& o) const { return Int128(value_ / o.value_); }
        Int128 operator%(const Int128& o) const { return Int128(value_ % o.value_); }
        Int128 operator&(const Int128& o) const { return Int128(value_ & o.value_); }
        Int128 operator|(const Int128& o) const { return Int128(value_ | o.value_); }
        Int128 operator^(const Int128& o) const { return Int128(value_ ^ o.value_); }
        Int128 operator~()               const { return Int128(~value_); }
        Int128 operator-()               const { return Int128(-value_); }
        Int128 operator<<(int n)         const { return Int128(value_ << n); }
        Int128 operator>>(int n)         const { return Int128(value_ >> n); }

        bool operator==(const Int128& o) const { return value_ == o.value_; }
        bool operator!=(const Int128& o) const { return value_ != o.value_; }
        bool operator< (const Int128& o) const { return value_ <  o.value_; }
        bool operator<=(const Int128& o) const { return value_ <= o.value_; }
        bool operator> (const Int128& o) const { return value_ >  o.value_; }
        bool operator>=(const Int128& o) const { return value_ >= o.value_; }

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
