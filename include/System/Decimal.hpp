// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>

namespace System {

/**
 * @brief 128-bit fixed-point decimal: 96-bit unsigned mantissa, scale 0–28, sign bit.
 *
 * Value = (-1)^sign × mantissa / 10^scale.
 * Matches .NET System.Decimal range: ±79,228,162,514,264,337,593,543,950,335.
 *
 * Partial C++ counterpart of .NET System.Decimal.
 *
 * @note Status: Partial — full arithmetic implemented; no implicit conversion from float.
 */
class Decimal {
    // 96-bit mantissa stored as a 128-bit GCC/Clang extension type.
    using u128 = unsigned __int128;
    static constexpr u128 MAX_MANTISSA = (u128(1) << 96) - 1; ///< 2^96 - 1

    u128    mantissa_ = 0;
    uint8_t scale_    = 0;
    bool    negative_ = false;

    // -----------------------------------------------------------------------
    // 192-bit helper type used by multiplication (three 64-bit limbs, LE).
    // -----------------------------------------------------------------------
    struct uint192 { uint64_t lo = 0, mid = 0, hi = 0; };

    // Private helpers (implemented in Decimal.cpp)
    static uint192  mul96x96(u128 a, u128 b);
    static uint32_t div192by10(uint192& v);
    static bool     fits96(const uint192& v);
    static u128     to128(const uint192& v);
    static void     alignScales(u128& m1, uint8_t& s1, u128& m2, uint8_t& s2);
    static std::string u128str(u128 v);
    static double   u128tod(u128 v);

    void fitMantissa();
    void normalize();

    /// Private constructor from raw internal parts.
    Decimal(u128 m, uint8_t s, bool n);

public:
    // -----------------------------------------------------------------------
    // Constructors
    // -----------------------------------------------------------------------

    /// @brief Initialises to zero.
    Decimal() = default;

    /// @brief Constructs from a signed 32-bit integer.
    /* implicit */ Decimal(int v);

    /// @brief Constructs from a signed 64-bit integer.
    /* implicit */ Decimal(long long v);

    /// @brief Constructs from a signed long (platform-dependent width).
    explicit Decimal(long v);

    /**
     * @brief Constructs from a double, approximating the value.
     *
     * Mirrors .NET behaviour: @c new Decimal(0.1) may differ from @c Parse("0.1").
     * @throws std::invalid_argument if @p v is NaN.
     * @throws std::overflow_error if @p v is Infinity or too large.
     */
    explicit Decimal(double v);

    // -----------------------------------------------------------------------
    // Manifest constants
    // -----------------------------------------------------------------------
    static const Decimal Zero;     ///< 0
    static const Decimal One;      ///< 1
    static const Decimal MinusOne; ///< -1
    static const Decimal MaxValue; ///< +79,228,162,514,264,337,593,543,950,335
    static const Decimal MinValue; ///< -79,228,162,514,264,337,593,543,950,335

    // -----------------------------------------------------------------------
    // Conversions
    // -----------------------------------------------------------------------
    [[nodiscard]] double    ToDouble() const;
    [[nodiscard]] float     ToSingle() const;
    [[nodiscard]] int       ToInt32()  const;
    [[nodiscard]] long long ToInt64()  const;

    // -----------------------------------------------------------------------
    // Parse / ToString
    // -----------------------------------------------------------------------

    /**
     * @brief Attempts to parse a decimal string. Accepts optional leading sign and
     *        a single decimal separator ('.' or ',').
     *
     * @param s       Input string.
     * @param result  Receives the parsed value on success.
     * @return @c true on success, @c false if the string is malformed or overflows.
     */
    static bool TryParse(const std::string& s, Decimal& result);

    /**
     * @brief Parses a decimal string.
     *
     * @throws std::invalid_argument if the string is malformed or overflows.
     */
    static Decimal Parse(const std::string& s);

    /**
     * @brief Returns a string representation of this value.
     *
     * Integer values have no decimal point; fractional values include exactly
     * @c scale_ digits after the decimal point.
     */
    [[nodiscard]] std::string ToString() const;

    // -----------------------------------------------------------------------
    // Arithmetic operators
    // -----------------------------------------------------------------------
    Decimal operator+(const Decimal& o) const;
    Decimal operator-(const Decimal& o) const;
    Decimal operator*(const Decimal& o) const;
    Decimal operator/(const Decimal& o) const;
    Decimal operator%(const Decimal& o) const;
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
    // Math helpers
    // -----------------------------------------------------------------------

    /// @brief Returns the absolute value of @p d.
    static Decimal Abs(const Decimal& d);

    /// @brief Truncates toward zero.
    static Decimal Truncate(const Decimal& d);

    /// @brief Returns the largest integer less than or equal to @p d.
    static Decimal Floor(const Decimal& d);

    /// @brief Returns the smallest integer greater than or equal to @p d.
    static Decimal Ceiling(const Decimal& d);

    /**
     * @brief Rounds to @p decimals decimal places using "round half up" (away from zero).
     *
     * @param d        Value to round.
     * @param decimals Number of decimal places (0–28).
     * @throws std::out_of_range if @p decimals is outside 0–28.
     */
    static Decimal Round(const Decimal& d, int decimals = 0);
};

} // namespace System
