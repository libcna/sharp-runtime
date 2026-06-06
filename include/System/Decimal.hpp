// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cmath>
#include <stdexcept>
#include <string>

namespace System {

// Simplified Decimal: backed by double; .NET Decimal has 128-bit precision
// which is not replicated here — use only for API compatibility.
class Decimal {
    double value_;

public:
    Decimal() : value_(0.0) {}
    explicit Decimal(double v) : value_(v) {}
    explicit Decimal(int v)    : value_(static_cast<double>(v)) {}
    explicit Decimal(long v)   : value_(static_cast<double>(v)) {}

    static const Decimal Zero;
    static const Decimal One;
    static const Decimal MinusOne;
    // .NET Decimal max/min expressed as doubles (not exact beyond double precision)
    static const Decimal MaxValue;
    static const Decimal MinValue;

    double ToDouble() const { return value_; }
    float  ToSingle() const { return static_cast<float>(value_); }
    int    ToInt32()  const { return static_cast<int>(std::trunc(value_)); }
    long   ToInt64()  const { return static_cast<long>(std::trunc(value_)); }

    static Decimal Parse(const std::string& s) {
        try { return Decimal(std::stod(s)); }
        catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    static bool TryParse(const std::string& s, Decimal& result) {
        try { result = Decimal(std::stod(s)); return true; }
        catch (...) { result = Decimal(); return false; }
    }

    Decimal operator+(const Decimal& o) const { return Decimal(value_ + o.value_); }
    Decimal operator-(const Decimal& o) const { return Decimal(value_ - o.value_); }
    Decimal operator*(const Decimal& o) const { return Decimal(value_ * o.value_); }
    Decimal operator/(const Decimal& o) const {
        if (o.value_ == 0.0) throw std::overflow_error("Attempted to divide by zero.");
        return Decimal(value_ / o.value_);
    }
    Decimal operator%(const Decimal& o) const { return Decimal(std::fmod(value_, o.value_)); }

    Decimal operator-() const { return Decimal(-value_); }

    bool operator==(const Decimal& o) const { return value_ == o.value_; }
    bool operator!=(const Decimal& o) const { return value_ != o.value_; }
    bool operator< (const Decimal& o) const { return value_ <  o.value_; }
    bool operator<=(const Decimal& o) const { return value_ <= o.value_; }
    bool operator> (const Decimal& o) const { return value_ >  o.value_; }
    bool operator>=(const Decimal& o) const { return value_ >= o.value_; }

    static Decimal Abs(const Decimal& d)   { return Decimal(std::abs(d.value_)); }
    static Decimal Floor(const Decimal& d) { return Decimal(std::floor(d.value_)); }
    static Decimal Ceiling(const Decimal& d){ return Decimal(std::ceil(d.value_)); }
    static Decimal Round(const Decimal& d, int decimals = 0) {
        double factor = std::pow(10.0, decimals);
        return Decimal(std::round(d.value_ * factor) / factor);
    }
    static Decimal Truncate(const Decimal& d) { return Decimal(std::trunc(d.value_)); }

    std::string ToString() const { return std::to_string(value_); }
};

inline const Decimal Decimal::Zero      = Decimal(0.0);
inline const Decimal Decimal::One       = Decimal(1.0);
inline const Decimal Decimal::MinusOne  = Decimal(-1.0);
inline const Decimal Decimal::MaxValue  = Decimal(7.9228162514264337593543950335e+28);
inline const Decimal Decimal::MinValue  = Decimal(-7.9228162514264337593543950335e+28);

} // namespace System
