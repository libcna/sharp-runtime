// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>

#if defined(_MSC_VER)
#  error "Decimal requires unsigned __int128 (GCC/Clang only). MSVC is not supported for this type."
#endif

namespace System {

/**
 * @brief 128-bit fixed-point decimal type matching .NET System.Decimal.
 *
 * Representation: value = (−1)^sign × mantissa / 10^scale,
 * where mantissa is a 96-bit unsigned integer and scale is 0–28.
 * This matches the .NET range: ±79,228,162,514,264,337,593,543,950,335.
 *
 * C++ counterpart of .NET System.Decimal.
 *
 * @note POSIX/GCC/Clang only — uses unsigned __int128 which MSVC does not support.
 */
class Decimal {
    using u128 = unsigned __int128;
    static constexpr u128 MAX_MANTISSA = (u128(1) << 96) - 1;

    u128    mantissa_ = 0;
    uint8_t scale_    = 0;
    bool    negative_ = false;

    struct uint192 { uint64_t lo = 0, mid = 0, hi = 0; };

    static uint192  mul96x96(u128 a, u128 b);
    static uint32_t div192by10(uint192& v);
    static bool     fits96(const uint192& v);
    static u128     to128(const uint192& v);
    static void     alignScales(u128& m1, uint8_t& s1, u128& m2, uint8_t& s2);
    static std::string u128str(u128 v);
    static double   u128tod(u128 v);

    void fitMantissa();
    void normalize();

    /** @brief Private constructor from raw internal parts. */
    Decimal(u128 m, uint8_t s, bool n);

public:
    // -----------------------------------------------------------------------
    // Constructors
    // -----------------------------------------------------------------------

    /** @brief Initializes a new instance of Decimal to zero. */
    Decimal() = default;

    /**
     * @brief Constructs a Decimal from a signed 32-bit integer.
     *
     * C++ counterpart of .NET Decimal(int).
     */
    /* implicit */ Decimal(int v);

    /**
     * @brief Constructs a Decimal from a signed 64-bit integer.
     *
     * C++ counterpart of .NET Decimal(long).
     */
    /* implicit */ Decimal(long long v);

    /**
     * @brief Constructs a Decimal from a signed long (platform-dependent width).
     */
    explicit Decimal(long v);

    /**
     * @brief Constructs a Decimal from a double-precision floating-point number.
     *
     * C++ counterpart of .NET Decimal(double).
     * Mirrors .NET behaviour: Decimal(0.1) may differ from Parse("0.1").
     * @throws std::invalid_argument if @p v is NaN.
     * @throws std::overflow_error if @p v is Infinity or too large.
     */
    explicit Decimal(double v);

    // -----------------------------------------------------------------------
    // Manifest constants
    // -----------------------------------------------------------------------

    /** @brief Represents the number zero (0). */
    static const Decimal Zero;

    /** @brief Represents the number one (1). */
    static const Decimal One;

    /** @brief Represents the number negative one (−1). */
    static const Decimal MinusOne;

    /** @brief Represents the largest possible value of a Decimal (+79,228,162,514,264,337,593,543,950,335). */
    static const Decimal MaxValue;

    /** @brief Represents the smallest possible value of a Decimal (−79,228,162,514,264,337,593,543,950,335). */
    static const Decimal MinValue;

    // -----------------------------------------------------------------------
    // Properties
    // -----------------------------------------------------------------------

    /**
     * @brief Gets the number of decimal places (0–28) in this instance.
     *
     * C++ counterpart of .NET Decimal.Scale.
     * @return The scale (number of digits to the right of the decimal point).
     */
    [[nodiscard]] uint8_t getScaleProperty() const noexcept { return scale_; }

    /**
     * @brief Gets a value indicating whether this instance is negative.
     *
     * C++ counterpart of checking the sign bit of a .NET Decimal.
     * @return true if the value is negative; otherwise false.
     */
    [[nodiscard]] bool getIsNegativeProperty() const noexcept { return negative_; }

    // -----------------------------------------------------------------------
    // Instance comparison / equality
    // -----------------------------------------------------------------------

    /**
     * @brief Compares this instance to another Decimal value.
     *
     * C++ counterpart of .NET Decimal.CompareTo(decimal).
     * @return A negative integer, zero, or a positive integer as this value is
     *         less than, equal to, or greater than @p other.
     */
    [[nodiscard]] int CompareTo(const Decimal& other) const
    {
        if (*this == other) return 0;
        return (*this < other) ? -1 : 1;
    }

    /**
     * @brief Determines whether this instance and @p other represent the same value.
     *
     * C++ counterpart of .NET Decimal.Equals(decimal).
     * @param other The Decimal to compare with this instance.
     * @return true if they are equal; otherwise false.
     */
    [[nodiscard]] bool Equals(const Decimal& other) const { return *this == other; }

    /**
     * @brief Returns a hash code for this Decimal value.
     *
     * C++ counterpart of .NET Decimal.GetHashCode().
     * Equal Decimal values (regardless of scale) produce the same hash code.
     * @return A 32-bit hash code.
     */
    [[nodiscard]] int GetHashCode() const noexcept
    {
        // Normalise: values that compare equal must have equal hashes, so
        // strip trailing zeros before hashing (same as .NET's behaviour).
        u128 m = mantissa_;
        uint8_t s = scale_;
        while (s > 0 && m % 10 == 0) { m /= 10; --s; }
        bool neg = negative_ && m != 0;
        size_t h = std::hash<uint64_t>{}(uint64_t(m));
        h ^= std::hash<uint64_t>{}(uint64_t(m >> 64)) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint8_t>{}(s)   + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<bool>{}(neg)    + 0x9e3779b9 + (h << 6) + (h >> 2);
        return static_cast<int>(h & 0x7fffffff);
    }

    // -----------------------------------------------------------------------
    // Conversions to primitive types
    // -----------------------------------------------------------------------

    /** @brief Converts this Decimal to a double-precision floating-point number. */
    [[nodiscard]] double    ToDouble() const;

    /** @brief Converts this Decimal to a single-precision floating-point number. */
    [[nodiscard]] float     ToSingle() const;

    /** @brief Converts this Decimal to a 32-bit signed integer, truncating any fractional part. */
    [[nodiscard]] int       ToInt32()  const;

    /** @brief Converts this Decimal to a 64-bit signed integer, truncating any fractional part. */
    [[nodiscard]] long long ToInt64()  const;

    /**
     * @brief Converts this Decimal to a 32-bit unsigned integer, truncating any fractional part.
     *
     * C++ counterpart of .NET Decimal.ToUInt32(decimal).
     * @throws std::overflow_error if the value is negative or exceeds uint32 range.
     */
    [[nodiscard]] uint32_t ToUInt32() const
    {
        long long v = ToInt64();
        if (v < 0 || v > UINT32_MAX)
            throw std::overflow_error("Decimal value is out of UInt32 range.");
        return static_cast<uint32_t>(v);
    }

    /**
     * @brief Converts this Decimal to a 64-bit unsigned integer, truncating any fractional part.
     *
     * C++ counterpart of .NET Decimal.ToUInt64(decimal).
     * @throws std::overflow_error if the value is negative or exceeds uint64 range.
     */
    [[nodiscard]] uint64_t ToUInt64() const
    {
        if (negative_ && mantissa_ != 0)
            throw std::overflow_error("Decimal value is out of UInt64 range.");
        Decimal t = Truncate(*this);
        return static_cast<uint64_t>(t.mantissa_);
    }

    // -----------------------------------------------------------------------
    // Parse / ToString
    // -----------------------------------------------------------------------

    /**
     * @brief Attempts to convert a string to a Decimal.
     *
     * C++ counterpart of .NET Decimal.TryParse(string, out decimal).
     * Accepts an optional leading sign and a single decimal separator ('.' or ',').
     * @param s      Input string.
     * @param result Receives the parsed value on success.
     * @return true on success; false if the string is malformed or overflows.
     */
    static bool TryParse(const std::string& s, Decimal& result);

    /**
     * @brief Converts the string representation of a number to its Decimal equivalent.
     *
     * C++ counterpart of .NET Decimal.Parse(string).
     * @throws std::invalid_argument if the string is malformed or overflows.
     */
    static Decimal Parse(const std::string& s);

    /**
     * @brief Converts the value of this instance to its equivalent string representation.
     *
     * C++ counterpart of .NET Decimal.ToString().
     * Integer values have no decimal point; fractional values include exactly
     * scale digits after the decimal point.
     */
    [[nodiscard]] std::string ToString() const;

    // -----------------------------------------------------------------------
    // Arithmetic operators
    // -----------------------------------------------------------------------

    /** @brief Adds two Decimal values. */
    Decimal operator+(const Decimal& o) const;
    /** @brief Subtracts one Decimal from another. */
    Decimal operator-(const Decimal& o) const;
    /** @brief Multiplies two Decimal values. */
    Decimal operator*(const Decimal& o) const;
    /** @brief Divides one Decimal by another. @throws std::overflow_error on division by zero. */
    Decimal operator/(const Decimal& o) const;
    /** @brief Returns the remainder of dividing two Decimal values. */
    Decimal operator%(const Decimal& o) const;
    /** @brief Returns the negation of a Decimal value. */
    Decimal operator-() const;

    Decimal& operator+=(const Decimal& o);
    Decimal& operator-=(const Decimal& o);
    Decimal& operator*=(const Decimal& o);
    Decimal& operator/=(const Decimal& o);

    // -----------------------------------------------------------------------
    // Comparison operators
    // -----------------------------------------------------------------------

    bool operator==(const Decimal& o) const;
    bool operator!=(const Decimal& o) const;
    bool operator< (const Decimal& o) const;
    bool operator<=(const Decimal& o) const;
    bool operator> (const Decimal& o) const;
    bool operator>=(const Decimal& o) const;

    // -----------------------------------------------------------------------
    // Static named arithmetic helpers (mirror .NET Decimal.Add etc.)
    // -----------------------------------------------------------------------

    /**
     * @brief Adds two Decimal values.
     *
     * C++ counterpart of .NET Decimal.Add(decimal, decimal).
     */
    static Decimal Add(const Decimal& d1, const Decimal& d2)      { return d1 + d2; }

    /**
     * @brief Subtracts one Decimal value from another.
     *
     * C++ counterpart of .NET Decimal.Subtract(decimal, decimal).
     */
    static Decimal Subtract(const Decimal& d1, const Decimal& d2) { return d1 - d2; }

    /**
     * @brief Multiplies two Decimal values.
     *
     * C++ counterpart of .NET Decimal.Multiply(decimal, decimal).
     */
    static Decimal Multiply(const Decimal& d1, const Decimal& d2) { return d1 * d2; }

    /**
     * @brief Divides one Decimal value by another.
     *
     * C++ counterpart of .NET Decimal.Divide(decimal, decimal).
     * @throws std::overflow_error on division by zero.
     */
    static Decimal Divide(const Decimal& d1, const Decimal& d2)   { return d1 / d2; }

    /**
     * @brief Computes the remainder after dividing one Decimal by another.
     *
     * C++ counterpart of .NET Decimal.Remainder(decimal, decimal).
     */
    static Decimal Remainder(const Decimal& d1, const Decimal& d2){ return d1 % d2; }

    /**
     * @brief Returns the result of multiplying the specified Decimal value by negative one.
     *
     * C++ counterpart of .NET Decimal.Negate(decimal).
     */
    static Decimal Negate(const Decimal& d)                       { return -d; }

    // -----------------------------------------------------------------------
    // Static comparison helper
    // -----------------------------------------------------------------------

    /**
     * @brief Compares two Decimal values.
     *
     * C++ counterpart of .NET Decimal.Compare(decimal, decimal).
     * @return A negative integer, zero, or a positive integer as @p d1 is
     *         less than, equal to, or greater than @p d2.
     */
    static int Compare(const Decimal& d1, const Decimal& d2)
    {
        return d1.CompareTo(d2);
    }

    // -----------------------------------------------------------------------
    // Math helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Returns the absolute value of a Decimal number.
     *
     * C++ counterpart of .NET Decimal.Abs(decimal).
     */
    static Decimal Abs(const Decimal& d);

    /**
     * @brief Returns the smallest integral value greater than or equal to @p d.
     *
     * C++ counterpart of .NET Decimal.Ceiling(decimal).
     */
    static Decimal Ceiling(const Decimal& d);

    /**
     * @brief Returns the largest integral value less than or equal to @p d.
     *
     * C++ counterpart of .NET Decimal.Floor(decimal).
     */
    static Decimal Floor(const Decimal& d);

    /**
     * @brief Returns the integral digits of @p d, discarding any fractional digits.
     *
     * C++ counterpart of .NET Decimal.Truncate(decimal).
     */
    static Decimal Truncate(const Decimal& d);

    /**
     * @brief Rounds @p d to @p decimals decimal places using "round half up" (away from zero).
     *
     * C++ counterpart of .NET Decimal.Round(decimal, int).
     * @param d        Value to round.
     * @param decimals Number of decimal places (0–28).
     * @throws std::out_of_range if @p decimals is outside 0–28.
     */
    static Decimal Round(const Decimal& d, int decimals = 0);

    /**
     * @brief Returns the larger of two Decimal values.
     *
     * C++ counterpart of .NET Math.Max / INumber.Max for Decimal.
     */
    static Decimal Max(const Decimal& d1, const Decimal& d2)
    {
        return (d1 >= d2) ? d1 : d2;
    }

    /**
     * @brief Returns the smaller of two Decimal values.
     *
     * C++ counterpart of .NET Math.Min / INumber.Min for Decimal.
     */
    static Decimal Min(const Decimal& d1, const Decimal& d2)
    {
        return (d1 <= d2) ? d1 : d2;
    }

    /**
     * @brief Returns a value clamped to the inclusive range [min, max].
     *
     * C++ counterpart of .NET Math.Clamp for Decimal.
     * @param value Value to clamp.
     * @param min   Minimum bound.
     * @param max   Maximum bound.
     */
    static Decimal Clamp(const Decimal& value, const Decimal& min, const Decimal& max)
    {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    /**
     * @brief Returns an integer that indicates the sign of a Decimal number.
     *
     * C++ counterpart of .NET Decimal.Sign / Math.Sign(decimal).
     * @return −1 if negative, 0 if zero, 1 if positive.
     */
    static int Sign(const Decimal& d)
    {
        if (d == Zero) return 0;
        return d.negative_ ? -1 : 1;
    }
};

} // namespace System
