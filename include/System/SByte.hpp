// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::sbytecs;

    /**
     * @brief Represents an 8-bit signed integer.
     *
     * C++ counterpart of .NET System.SByte.
     * All members are static; the class cannot be instantiated.
     * The underlying C++ type is @c int8_t (aliased as @c SharpRuntime::sbytecs).
     */
    class SByte {
    public:
        SByte() = delete;

        /** @brief The maximum value of an SByte (127). C++ counterpart of .NET SByte.MaxValue. */
        static constexpr sbytecs MaxValue = std::numeric_limits<int8_t>::max();

        /** @brief The minimum value of an SByte (-128). C++ counterpart of .NET SByte.MinValue. */
        static constexpr sbytecs MinValue = std::numeric_limits<int8_t>::min();

        /**
         * @brief Parses @p s as a decimal SByte value.
         * C++ counterpart of .NET SByte.Parse(string).
         * @throws std::out_of_range if the value exceeds SByte range.
         * @throws std::invalid_argument if the string is not a valid integer.
         */
        [[nodiscard]] static sbytecs Parse(const std::string& s) {
            try {
                int v = std::stoi(s);
                if (v < MinValue || v > MaxValue)
                    throw std::out_of_range("Value out of SByte range.");
                return static_cast<int8_t>(v);
            } catch (const std::out_of_range&) { throw; }
              catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
        }

        /**
         * @brief Attempts to parse @p s as an SByte; returns false on failure.
         * C++ counterpart of .NET SByte.TryParse(string, out sbyte).
         */
        static bool TryParse(const std::string& s, sbytecs& result) noexcept {
            try { result = Parse(s); return true; }
            catch (...) { result = 0; return false; }
        }

        /** @brief Converts @p value to its decimal string representation. C++ counterpart of .NET SByte.ToString(). */
        [[nodiscard]] static std::string ToString(sbytecs value) {
            return std::to_string(static_cast<int>(value));
        }

        /** @brief Converts @p value to a string using format specifier ("X","x","D","d","G","g"). */
        [[nodiscard]] static std::string ToString(sbytecs value, const std::string& format) {
            if (format.empty()) return ToString(value);
            char type = format[0];
            int  width = format.size() > 1 ? std::stoi(format.substr(1)) : 0;
            std::ostringstream oss;
            if (type == 'X') {
                oss << std::uppercase << std::hex
                    << std::setfill('0') << std::setw(width)
                    << (static_cast<unsigned>(value) & 0xFFu);
                return oss.str();
            }
            if (type == 'x') {
                oss << std::hex << std::setfill('0') << std::setw(width)
                    << (static_cast<unsigned>(value) & 0xFFu);
                return oss.str();
            }
            if (type == 'D' || type == 'd') {
                bool neg = value < 0;
                std::string s = std::to_string(neg ? -static_cast<int>(value) : static_cast<int>(value));
                while (static_cast<int>(s.size()) < width) s = "0" + s;
                return neg ? "-" + s : s;
            }
            if (type == 'G' || type == 'g') return ToString(value);
            return ToString(value);
        }

        /** @brief Compares @p a to @p b. C++ counterpart of .NET SByte.CompareTo(sbyte). */
        [[nodiscard]] static int CompareTo(sbytecs a, sbytecs b) noexcept {
            return static_cast<int>(a) - static_cast<int>(b);
        }

        /** @brief Returns true if @p a equals @p b. C++ counterpart of .NET SByte.Equals(sbyte). */
        [[nodiscard]] static bool Equals(sbytecs a, sbytecs b) noexcept { return a == b; }

        /** @brief Returns a hash code for @p value. C++ counterpart of .NET SByte.GetHashCode(). */
        [[nodiscard]] static int GetHashCode(sbytecs value) noexcept { return static_cast<int>(value); }

        /** @brief Clamps @p value to [@p min, @p max]. C++ counterpart of .NET SByte.Clamp(sbyte,sbyte,sbyte). */
        [[nodiscard]] static sbytecs Clamp(sbytecs value, sbytecs min, sbytecs max) noexcept {
            return std::clamp(value, min, max);
        }

        /** @brief Returns the larger of @p x and @p y. C++ counterpart of .NET SByte.Max(sbyte,sbyte). */
        [[nodiscard]] static sbytecs Max(sbytecs x, sbytecs y) noexcept { return x > y ? x : y; }

        /** @brief Returns the smaller of @p x and @p y. C++ counterpart of .NET SByte.Min(sbyte,sbyte). */
        [[nodiscard]] static sbytecs Min(sbytecs x, sbytecs y) noexcept { return x < y ? x : y; }

        /** @brief Returns -1 if negative, 0 if zero, 1 if positive. C++ counterpart of .NET Math.Sign(sbyte). */
        [[nodiscard]] static int Sign(sbytecs value) noexcept {
            return (value > 0) - (value < 0);
        }

        /** @brief Returns the absolute value of @p value. C++ counterpart of .NET SByte.Abs(sbyte). */
        [[nodiscard]] static sbytecs Abs(sbytecs value) {
            if (value == MinValue)
                throw std::overflow_error("Negating SByte.MinValue would overflow.");
            return value < 0 ? static_cast<sbytecs>(-value) : value;
        }

        /**
         * @brief Returns a value with the magnitude of @p value and the sign of @p sign.
         * C++ counterpart of .NET SByte.CopySign(sbyte, sbyte).
         */
        [[nodiscard]] static sbytecs CopySign(sbytecs value, sbytecs sign) noexcept {
            sbytecs abs = value < 0 ? static_cast<sbytecs>(-value) : value;
            return sign < 0 ? static_cast<sbytecs>(-abs) : abs;
        }

        /** @brief Returns true if @p value is negative. C++ counterpart of .NET SByte.IsNegative(sbyte). */
        [[nodiscard]] static bool IsNegative(sbytecs value) noexcept { return value < 0; }

        /** @brief Returns true if @p value is positive (> 0). C++ counterpart of .NET SByte.IsPositive(sbyte). */
        [[nodiscard]] static bool IsPositive(sbytecs value) noexcept { return value > 0; }

        /**
         * @brief Returns the base-10 logarithm of @p value, truncated to sbyte.
         * C++ counterpart of .NET SByte.Log10(sbyte).
         * @throws std::domain_error if value is <= 0.
         */
        [[nodiscard]] static sbytecs Log10(sbytecs value) {
            if (value <= 0) throw std::domain_error("Log10 requires a positive value.");
            sbytecs result = 0;
            while (value >= 10) { value /= 10; ++result; }
            return result;
        }

        /**
         * @brief Returns the value with greater magnitude; if magnitudes are equal, returns @p x.
         * C++ counterpart of .NET SByte.MaxMagnitude(sbyte, sbyte).
         */
        [[nodiscard]] static sbytecs MaxMagnitude(sbytecs x, sbytecs y) noexcept {
            sbytecs ax = x < 0 ? static_cast<sbytecs>(-x) : x;
            sbytecs ay = y < 0 ? static_cast<sbytecs>(-y) : y;
            return ax >= ay ? x : y;
        }

        /**
         * @brief Returns the value with smaller magnitude; if magnitudes are equal, returns @p x.
         * C++ counterpart of .NET SByte.MinMagnitude(sbyte, sbyte).
         */
        [[nodiscard]] static sbytecs MinMagnitude(sbytecs x, sbytecs y) noexcept {
            sbytecs ax = x < 0 ? static_cast<sbytecs>(-x) : x;
            sbytecs ay = y < 0 ? static_cast<sbytecs>(-y) : y;
            return ax <= ay ? x : y;
        }

        /**
         * @brief Rotates @p value left by @p rotateAmount bits within an 8-bit field.
         * C++ counterpart of .NET SByte.RotateLeft(sbyte, int).
         */
        [[nodiscard]] static sbytecs RotateLeft(sbytecs value, int rotateAmount) noexcept {
            uint8_t uv = static_cast<uint8_t>(value);
            int shift = rotateAmount & 7;
            return static_cast<sbytecs>((uv << shift) | (uv >> (8 - shift)));
        }

        /**
         * @brief Rotates @p value right by @p rotateAmount bits within an 8-bit field.
         * C++ counterpart of .NET SByte.RotateRight(sbyte, int).
         */
        [[nodiscard]] static sbytecs RotateRight(sbytecs value, int rotateAmount) noexcept {
            uint8_t uv = static_cast<uint8_t>(value);
            int shift = rotateAmount & 7;
            return static_cast<sbytecs>((uv >> shift) | (uv << (8 - shift)));
        }

        /**
         * @brief Returns the quotient and remainder of @p left / @p right.
         * C++ counterpart of .NET SByte.DivRem(sbyte,sbyte).
         */
        [[nodiscard]] static std::pair<sbytecs, sbytecs> DivRem(sbytecs left, sbytecs right) {
            return {static_cast<sbytecs>(left / right),
                    static_cast<sbytecs>(left % right)};
        }

        /** @brief Returns true when @p value is even. C++ counterpart of .NET SByte.IsEvenInteger(sbyte). */
        [[nodiscard]] static bool IsEvenInteger(sbytecs value) noexcept { return (value & 1) == 0; }

        /** @brief Returns true when @p value is odd. C++ counterpart of .NET SByte.IsOddInteger(sbyte). */
        [[nodiscard]] static bool IsOddInteger(sbytecs value) noexcept { return (value & 1) != 0; }

        /** @brief Returns true when @p value is a power of two. C++ counterpart of .NET SByte.IsPow2(sbyte). */
        [[nodiscard]] static bool IsPow2(sbytecs value) noexcept {
            return value > 0 && (value & (value - 1)) == 0;
        }

        /** @brief Returns the number of leading zero bits (treating value as 8-bit). C++ counterpart of .NET SByte.LeadingZeroCount(sbyte). */
        [[nodiscard]] static sbytecs LeadingZeroCount(sbytecs value) noexcept {
            if (value <= 0) return 8;
            return static_cast<sbytecs>(
                std::countl_zero(static_cast<uint32_t>(static_cast<uint8_t>(value))) - 24);
        }

        /** @brief Returns the number of set bits. C++ counterpart of .NET SByte.PopCount(sbyte). */
        [[nodiscard]] static sbytecs PopCount(sbytecs value) noexcept {
            return static_cast<sbytecs>(
                std::popcount(static_cast<uint8_t>(value)));
        }

        /** @brief Returns the number of trailing zero bits. C++ counterpart of .NET SByte.TrailingZeroCount(sbyte). */
        [[nodiscard]] static sbytecs TrailingZeroCount(sbytecs value) noexcept {
            if (value == 0) return 8;
            return static_cast<sbytecs>(
                std::countr_zero(static_cast<uint32_t>(static_cast<uint8_t>(value))));
        }

        /**
         * @brief Returns the floor of the base-2 logarithm of @p value.
         * C++ counterpart of .NET SByte.Log2(sbyte).
         * @throws std::domain_error if @p value is <= 0.
         */
        [[nodiscard]] static sbytecs Log2(sbytecs value) {
            if (value <= 0) throw std::domain_error("Log2 requires a positive value.");
            return static_cast<sbytecs>(
                std::bit_width(static_cast<uint32_t>(static_cast<uint8_t>(value))) - 1);
        }
    };

} // namespace System
