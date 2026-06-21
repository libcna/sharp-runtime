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

    using SharpRuntime::longcs;

    /**
     * @brief Represents a 64-bit signed integer.
     *
     * C++ counterpart of .NET System.Int64.
     * All members are static; the class cannot be instantiated.
     * The underlying C++ type is @c int64_t (aliased as @c SharpRuntime::longcs).
     */
    class Int64 {
    public:
        Int64() = delete;

        /** @brief The maximum value of an Int64 (9 223 372 036 854 775 807). C++ counterpart of .NET Int64.MaxValue. */
        static constexpr longcs MaxValue = std::numeric_limits<int64_t>::max();

        /** @brief The minimum value of an Int64 (-9 223 372 036 854 775 808). C++ counterpart of .NET Int64.MinValue. */
        static constexpr longcs MinValue = std::numeric_limits<int64_t>::min();

        /**
         * @brief Parses @p s as a decimal Int64 value.
         * C++ counterpart of .NET Int64.Parse(string).
         * @throws std::invalid_argument on bad format.
         */
        [[nodiscard]] static longcs Parse(const std::string& s) {
            try { return static_cast<int64_t>(std::stoll(s)); }
            catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
        }

        /**
         * @brief Attempts to parse @p s as an Int64; returns false on failure.
         * C++ counterpart of .NET Int64.TryParse(string, out long).
         */
        static bool TryParse(const std::string& s, longcs& result) noexcept {
            try { result = static_cast<int64_t>(std::stoll(s)); return true; }
            catch (...) { result = 0; return false; }
        }

        /** @brief Converts @p value to its decimal string representation. C++ counterpart of .NET Int64.ToString(). */
        [[nodiscard]] static std::string ToString(longcs value) { return std::to_string(value); }

        /**
         * @brief Converts @p value to a string using format specifier ("X","x","D","d","G","g").
         * C++ counterpart of .NET Int64.ToString(string).
         */
        [[nodiscard]] static std::string ToString(longcs value, const std::string& format) {
            if (format.empty()) return std::to_string(value);
            char type = format[0];
            int width = format.size() > 1 ? std::stoi(format.substr(1)) : 0;
            std::ostringstream oss;
            if (type == 'X') {
                oss << std::uppercase << std::hex;
                if (width > 0) oss << std::setfill('0') << std::setw(width);
                oss << static_cast<uint64_t>(value);
                return oss.str();
            }
            if (type == 'x') {
                oss << std::hex;
                if (width > 0) oss << std::setfill('0') << std::setw(width);
                oss << static_cast<uint64_t>(value);
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
            return std::to_string(value);
        }

        /**
         * @brief Compares @p a to @p b and returns a signed integer.
         * C++ counterpart of .NET Int64.CompareTo(long).
         */
        [[nodiscard]] static int CompareTo(longcs a, longcs b) noexcept {
            return (a < b) ? -1 : (a > b) ? 1 : 0;
        }

        /** @brief Returns true if @p a equals @p b. C++ counterpart of .NET Int64.Equals(long). */
        [[nodiscard]] static bool Equals(longcs a, longcs b) noexcept { return a == b; }

        /** @brief Returns a hash code for @p value. C++ counterpart of .NET Int64.GetHashCode(). */
        [[nodiscard]] static int GetHashCode(longcs value) noexcept {
            return static_cast<int>(value ^ (static_cast<uint64_t>(value) >> 32));
        }

        /** @brief Returns the absolute value of @p value. C++ counterpart of .NET Math.Abs(long). */
        [[nodiscard]] static longcs Abs(longcs value) {
            if (value == MinValue) throw std::overflow_error("Abs of Int64.MinValue overflows.");
            return value < 0 ? -value : value;
        }

        /** @brief Clamps @p value to [@p min, @p max]. C++ counterpart of .NET Int64.Clamp(long,long,long). */
        [[nodiscard]] static longcs Clamp(longcs value, longcs min, longcs max) noexcept {
            return std::clamp(value, min, max);
        }

        /** @brief Returns the larger of @p x and @p y. C++ counterpart of .NET Int64.Max(long,long). */
        [[nodiscard]] static longcs Max(longcs x, longcs y) noexcept { return x > y ? x : y; }

        /** @brief Returns the smaller of @p x and @p y. C++ counterpart of .NET Int64.Min(long,long). */
        [[nodiscard]] static longcs Min(longcs x, longcs y) noexcept { return x < y ? x : y; }

        /** @brief Returns -1 if negative, 0 if zero, 1 if positive. C++ counterpart of .NET Math.Sign(long). */
        [[nodiscard]] static int Sign(longcs value) noexcept {
            return (value > 0) - (value < 0);
        }

        /**
         * @brief Returns the quotient and remainder of @p left / @p right.
         * C++ counterpart of .NET Int64.DivRem(long,long).
         */
        [[nodiscard]] static std::pair<longcs, longcs> DivRem(longcs left, longcs right) {
            return {left / right, left % right};
        }

        /** @brief Returns true when @p value is even. C++ counterpart of .NET Int64.IsEvenInteger(long). */
        [[nodiscard]] static bool IsEvenInteger(longcs value) noexcept { return (value & 1) == 0; }

        /** @brief Returns true when @p value is odd. C++ counterpart of .NET Int64.IsOddInteger(long). */
        [[nodiscard]] static bool IsOddInteger(longcs value) noexcept { return (value & 1) != 0; }

        /** @brief Returns true when @p value is a power of two. C++ counterpart of .NET Int64.IsPow2(long). */
        [[nodiscard]] static bool IsPow2(longcs value) noexcept {
            return value > 0 && (value & (value - 1)) == 0;
        }

        /** @brief Returns the number of leading zero bits. C++ counterpart of .NET Int64.LeadingZeroCount(long). */
        [[nodiscard]] static int LeadingZeroCount(longcs value) noexcept {
            return std::countl_zero(static_cast<uint64_t>(value));
        }

        /** @brief Returns the number of set bits. C++ counterpart of .NET Int64.PopCount(long). */
        [[nodiscard]] static int PopCount(longcs value) noexcept {
            return std::popcount(static_cast<uint64_t>(value));
        }

        /** @brief Returns the number of trailing zero bits. C++ counterpart of .NET Int64.TrailingZeroCount(long). */
        [[nodiscard]] static int TrailingZeroCount(longcs value) noexcept {
            if (value == 0) return 64;
            return std::countr_zero(static_cast<uint64_t>(value));
        }

        /** @brief Rotates @p value left by @p rotateAmount bits. C++ counterpart of .NET Int64.RotateLeft(long,int). */
        [[nodiscard]] static longcs RotateLeft(longcs value, int rotateAmount) noexcept {
            return static_cast<longcs>(
                std::rotl(static_cast<uint64_t>(value), rotateAmount));
        }

        /** @brief Rotates @p value right by @p rotateAmount bits. C++ counterpart of .NET Int64.RotateRight(long,int). */
        [[nodiscard]] static longcs RotateRight(longcs value, int rotateAmount) noexcept {
            return static_cast<longcs>(
                std::rotr(static_cast<uint64_t>(value), rotateAmount));
        }

        /**
         * @brief Returns the floor of the base-2 logarithm of @p value.
         * C++ counterpart of .NET Int64.Log2(long).
         * @throws std::domain_error if @p value is <= 0.
         */
        [[nodiscard]] static int Log2(longcs value) {
            if (value <= 0) throw std::domain_error("Log2 requires a positive value.");
            return std::bit_width(static_cast<uint64_t>(value)) - 1;
        }
    };

} // namespace System

