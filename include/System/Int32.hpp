// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

/**
 * @brief Static helper methods that mirror System.Int32 in .NET.
 *
 * C++ counterpart of .NET System.Int32.
 * Represents a 32-bit signed integer and exposes the static utility methods
 * of the .NET Int32 struct as static C++ methods.
 */
class Int32 {
public:
    /** @brief Maximum value of a 32-bit signed integer (2 147 483 647). */
    static constexpr SharpRuntime::intcs MaxValue = std::numeric_limits<int32_t>::max();
    /** @brief Minimum value of a 32-bit signed integer (-2 147 483 648). */
    static constexpr SharpRuntime::intcs MinValue = std::numeric_limits<int32_t>::min();

    // -----------------------------------------------------------------------
    // Parse / TryParse
    // -----------------------------------------------------------------------

    /**
     * @brief Converts the string representation of a number to its 32-bit
     * signed integer equivalent.
     *
     * C++ counterpart of .NET Int32.Parse(string).
     * @param s String to parse.
     * @return Parsed int32 value.
     * @throws std::invalid_argument if the string is not a valid integer.
     */
    static SharpRuntime::intcs Parse(const std::string& s) {
        try { return static_cast<int32_t>(std::stoi(s)); }
        catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
    }

    /**
     * @brief Tries to convert a string to a 32-bit signed integer without
     * throwing.
     *
     * C++ counterpart of .NET Int32.TryParse(string, out int).
     * @param s      String to parse.
     * @param result Receives the parsed value on success, or 0 on failure.
     * @return True if parsing succeeded; false otherwise.
     */
    static bool TryParse(const std::string& s, SharpRuntime::intcs& result) {
        try { result = static_cast<int32_t>(std::stoi(s)); return true; }
        catch (...) { result = 0; return false; }
    }

    // -----------------------------------------------------------------------
    // ToString
    // -----------------------------------------------------------------------

    /**
     * @brief Converts the 32-bit signed integer @p value to its decimal
     * string representation.
     *
     * C++ counterpart of .NET Int32.ToString().
     */
    static std::string ToString(SharpRuntime::intcs value) {
        return std::to_string(value);
    }

    /**
     * @brief Converts @p value to a string using the specified format specifier.
     *
     * C++ counterpart of .NET Int32.ToString(string format).
     * Supported specifiers:
     *   - @c "X" / @c "x" — hexadecimal (uppercase/lowercase), optional digit width
     *   - @c "D" / @c "d" — decimal with optional zero-padded minimum width
     *   - @c "G" / @c "g" — general (decimal)
     *   - @c "B" / @c "b" — binary, optional minimum digit width
     */
    static std::string ToString(SharpRuntime::intcs value, const std::string& format) {
        if (format.empty()) return std::to_string(value);
        char type = format[0];
        int width = format.size() > 1 ? std::stoi(format.substr(1)) : 0;
        std::ostringstream oss;
        if (type == 'X') {
            oss << std::uppercase << std::hex;
            if (width > 0) oss << std::setfill('0') << std::setw(width);
            oss << static_cast<unsigned>(value);
            return oss.str();
        }
        if (type == 'x') {
            oss << std::hex;
            if (width > 0) oss << std::setfill('0') << std::setw(width);
            oss << static_cast<unsigned>(value);
            return oss.str();
        }
        if (type == 'D' || type == 'd') {
            if (width > 0) {
                std::string s = std::to_string(value);
                bool neg = value < 0;
                if (neg) s = s.substr(1);
                while (static_cast<int>(s.size()) < width) s = "0" + s;
                return neg ? "-" + s : s;
            }
            return std::to_string(value);
        }
        if (type == 'G' || type == 'g') return std::to_string(value);
        if (type == 'B' || type == 'b') {
            if (value == 0) {
                return std::string(width > 0 ? static_cast<size_t>(width) : 1u, '0');
            }
            std::string bits;
            uint32_t uv = static_cast<uint32_t>(value);
            while (uv > 0) { bits = static_cast<char>('0' + (uv & 1)) + bits; uv >>= 1; }
            while (static_cast<int>(bits.size()) < width) bits = "0" + bits;
            return bits;
        }
        return std::to_string(value);
    }

    // -----------------------------------------------------------------------
    // Arithmetic helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Produces the full 64-bit product of two 32-bit numbers.
     *
     * C++ counterpart of .NET Int32.BigMul(int, int).
     * @param left  The first number to multiply.
     * @param right The second number to multiply.
     * @return The 64-bit product.
     */
    [[nodiscard]] static SharpRuntime::longcs BigMul(
            SharpRuntime::intcs left, SharpRuntime::intcs right) noexcept {
        return static_cast<SharpRuntime::longcs>(left)
             * static_cast<SharpRuntime::longcs>(right);
    }

    /**
     * @brief Computes the quotient and remainder of two 32-bit integers.
     *
     * C++ counterpart of .NET Int32.DivRem(int, int).
     * @param left  The dividend.
     * @param right The divisor.
     * @return A pair {Quotient, Remainder}.
     */
    [[nodiscard]] static std::pair<SharpRuntime::intcs, SharpRuntime::intcs>
    DivRem(SharpRuntime::intcs left, SharpRuntime::intcs right) noexcept {
        return { left / right, left % right };
    }

    /**
     * @brief Returns the absolute value of a 32-bit signed integer.
     *
     * C++ counterpart of .NET Int32.Abs(int).
     */
    [[nodiscard]] static SharpRuntime::intcs Abs(SharpRuntime::intcs value) {
        if (value == MinValue)
            throw std::overflow_error("Negating MinValue is not representable.");
        return value < 0 ? -value : value;
    }

    /**
     * @brief Clamps @p value to the inclusive range [@p min, @p max].
     *
     * C++ counterpart of .NET Int32.Clamp(int, int, int).
     */
    [[nodiscard]] static SharpRuntime::intcs Clamp(
            SharpRuntime::intcs value,
            SharpRuntime::intcs min,
            SharpRuntime::intcs max) noexcept {
        return std::clamp(value, min, max);
    }

    /**
     * @brief Copies the sign of @p sign to the magnitude of @p value.
     *
     * C++ counterpart of .NET Int32.CopySign(int, int).
     */
    [[nodiscard]] static SharpRuntime::intcs CopySign(
            SharpRuntime::intcs value, SharpRuntime::intcs sign) {
        int32_t absValue = value < 0 ? (value == MinValue ? value : -value) : value;
        if (sign >= 0) {
            if (absValue < 0)
                throw std::overflow_error("Negating MinValue is not representable.");
            return absValue;
        }
        return -absValue;
    }

    /**
     * @brief Returns the larger of two 32-bit integers.
     *
     * C++ counterpart of .NET Int32.Max(int, int).
     */
    [[nodiscard]] static SharpRuntime::intcs Max(
            SharpRuntime::intcs x, SharpRuntime::intcs y) noexcept {
        return x > y ? x : y;
    }

    /**
     * @brief Returns the smaller of two 32-bit integers.
     *
     * C++ counterpart of .NET Int32.Min(int, int).
     */
    [[nodiscard]] static SharpRuntime::intcs Min(
            SharpRuntime::intcs x, SharpRuntime::intcs y) noexcept {
        return x < y ? x : y;
    }

    /**
     * @brief Returns the integer with the larger absolute value.
     *
     * C++ counterpart of .NET Int32.MaxMagnitude(int, int).
     * If both have equal magnitude, returns the positive one.
     */
    [[nodiscard]] static SharpRuntime::intcs MaxMagnitude(
            SharpRuntime::intcs x, SharpRuntime::intcs y) noexcept {
        int32_t ax = x < 0 ? (x == MinValue ? x : -x) : x;
        int32_t ay = y < 0 ? (y == MinValue ? y : -y) : y;
        if (ax > ay) return x;
        if (ax == ay) return IsNegative(x) ? y : x;
        return y;
    }

    /**
     * @brief Returns the integer with the smaller absolute value.
     *
     * C++ counterpart of .NET Int32.MinMagnitude(int, int).
     * If both have equal magnitude, returns the negative one.
     */
    [[nodiscard]] static SharpRuntime::intcs MinMagnitude(
            SharpRuntime::intcs x, SharpRuntime::intcs y) noexcept {
        int32_t ax = x < 0 ? (x == MinValue ? x : -x) : x;
        int32_t ay = y < 0 ? (y == MinValue ? y : -y) : y;
        if (ax < ay) return x;
        if (ax == ay) return IsNegative(x) ? x : y;
        return y;
    }

    /**
     * @brief Returns a value indicating the sign of a 32-bit signed integer.
     *
     * C++ counterpart of .NET Int32.Sign(int).
     * @return -1 if @p value is negative, 0 if zero, 1 if positive.
     */
    [[nodiscard]] static SharpRuntime::intcs Sign(
            SharpRuntime::intcs value) noexcept {
        return value < 0 ? -1 : (value > 0 ? 1 : 0);
    }

    // -----------------------------------------------------------------------
    // Bit operations
    // -----------------------------------------------------------------------

    /**
     * @brief Returns the number of leading zero bits in a 32-bit integer.
     *
     * C++ counterpart of .NET Int32.LeadingZeroCount(int).
     */
    [[nodiscard]] static SharpRuntime::intcs LeadingZeroCount(
            SharpRuntime::intcs value) noexcept {
        return static_cast<SharpRuntime::intcs>(
            std::countl_zero(static_cast<uint32_t>(value)));
    }

    /**
     * @brief Returns the number of trailing zero bits in a 32-bit integer.
     *
     * C++ counterpart of .NET Int32.TrailingZeroCount(int).
     */
    [[nodiscard]] static SharpRuntime::intcs TrailingZeroCount(
            SharpRuntime::intcs value) noexcept {
        return static_cast<SharpRuntime::intcs>(
            std::countr_zero(static_cast<uint32_t>(value)));
    }

    /**
     * @brief Returns the number of set bits (population count) in a 32-bit integer.
     *
     * C++ counterpart of .NET Int32.PopCount(int).
     */
    [[nodiscard]] static SharpRuntime::intcs PopCount(
            SharpRuntime::intcs value) noexcept {
        return static_cast<SharpRuntime::intcs>(
            std::popcount(static_cast<uint32_t>(value)));
    }

    /**
     * @brief Rotates the bits of a 32-bit integer left by @p rotateAmount positions.
     *
     * C++ counterpart of .NET Int32.RotateLeft(int, int).
     */
    [[nodiscard]] static SharpRuntime::intcs RotateLeft(
            SharpRuntime::intcs value, SharpRuntime::intcs rotateAmount) noexcept {
        return static_cast<SharpRuntime::intcs>(
            std::rotl(static_cast<uint32_t>(value),
                      static_cast<int>(rotateAmount)));
    }

    /**
     * @brief Rotates the bits of a 32-bit integer right by @p rotateAmount positions.
     *
     * C++ counterpart of .NET Int32.RotateRight(int, int).
     */
    [[nodiscard]] static SharpRuntime::intcs RotateRight(
            SharpRuntime::intcs value, SharpRuntime::intcs rotateAmount) noexcept {
        return static_cast<SharpRuntime::intcs>(
            std::rotr(static_cast<uint32_t>(value),
                      static_cast<int>(rotateAmount)));
    }

    /**
     * @brief Determines whether @p value is a power of two.
     *
     * C++ counterpart of .NET Int32.IsPow2(int).
     */
    [[nodiscard]] static bool IsPow2(SharpRuntime::intcs value) noexcept {
        return value > 0 && std::has_single_bit(static_cast<uint32_t>(value));
    }

    /**
     * @brief Returns the integer base-2 logarithm of a 32-bit integer.
     *
     * C++ counterpart of .NET Int32.Log2(int).
     * @throws std::invalid_argument if @p value is negative.
     */
    [[nodiscard]] static SharpRuntime::intcs Log2(SharpRuntime::intcs value) {
        if (value < 0)
            throw std::invalid_argument("value must be non-negative.");
        if (value == 0) return 0;
        return static_cast<SharpRuntime::intcs>(std::bit_width(static_cast<uint32_t>(value)) - 1);
    }

    // -----------------------------------------------------------------------
    // Classification predicates
    // -----------------------------------------------------------------------

    /**
     * @brief Returns true if @p value is an even integer.
     *
     * C++ counterpart of .NET Int32.IsEvenInteger(int).
     */
    [[nodiscard]] static bool IsEvenInteger(SharpRuntime::intcs value) noexcept {
        return (value & 1) == 0;
    }

    /**
     * @brief Returns true if @p value is an odd integer.
     *
     * C++ counterpart of .NET Int32.IsOddInteger(int).
     */
    [[nodiscard]] static bool IsOddInteger(SharpRuntime::intcs value) noexcept {
        return (value & 1) != 0;
    }

    /**
     * @brief Returns true if @p value is negative.
     *
     * C++ counterpart of .NET Int32.IsNegative(int).
     */
    [[nodiscard]] static bool IsNegative(SharpRuntime::intcs value) noexcept {
        return value < 0;
    }

    /**
     * @brief Returns true if @p value is zero or positive.
     *
     * C++ counterpart of .NET Int32.IsPositive(int).
     */
    [[nodiscard]] static bool IsPositive(SharpRuntime::intcs value) noexcept {
        return value >= 0;
    }
};

} // namespace System
