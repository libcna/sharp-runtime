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

    using SharpRuntime::intcs;

    /**
     * @brief Provides static helper methods that mirror .NET System.UInt64.
     *
     * Represents a 64-bit unsigned integer (0 to 18446744073709551615).
     */
    class UInt64 {
    public:
        /** @brief Maximum value of a 64-bit unsigned integer (18446744073709551615). */
        static constexpr SharpRuntime::ulongcs MaxValue = std::numeric_limits<uint64_t>::max();
        /** @brief Minimum value of a 64-bit unsigned integer (0). */
        static constexpr SharpRuntime::ulongcs MinValue = 0;

        /**
         * @brief Converts the string representation of a number to its UInt64 equivalent.
         * @throws std::invalid_argument if the string is not a valid integer.
         */
        static SharpRuntime::ulongcs Parse(const std::string& s) {
            try { return std::stoull(s); }
            catch (...) { throw std::invalid_argument("Input string was not in a correct format."); }
        }

        /**
         * @brief Tries to convert a string to a UInt64 without throwing.
         * @param result Receives the parsed value on success, or 0 on failure.
         * @return true if parsing succeeded; false otherwise.
         */
        static bool TryParse(const std::string& s, SharpRuntime::ulongcs& result) {
            try { result = Parse(s); return true; }
            catch (...) { result = 0; return false; }
        }

        /** @brief Converts the value to its decimal string representation. */
        static std::string ToString(SharpRuntime::ulongcs value) { return std::to_string(value); }

        /** @brief Converts value to a string using a format specifier ("X", "X16", "D", "D20", "G"). */
        static std::string ToString(SharpRuntime::ulongcs value, const std::string& format) {
            if (format.empty()) return ToString(value);
            char type = format[0];
            int width = format.size() > 1 ? std::stoi(format.substr(1)) : 0;
            std::ostringstream oss;
            if (type == 'X') { oss << std::uppercase << std::hex << std::setfill('0') << std::setw(width) << value; return oss.str(); }
            if (type == 'x') { oss << std::hex << std::setfill('0') << std::setw(width) << value; return oss.str(); }
            if (type == 'D' || type == 'd') {
                std::string s = std::to_string(value);
                while (static_cast<int>(s.size()) < width) s = "0" + s;
                return s;
            }
            if (type == 'G' || type == 'g') return ToString(value);
            return ToString(value);
        }

        /** @brief Compares two UInt64 values. Returns negative, zero, or positive. */
        static intcs CompareTo(SharpRuntime::ulongcs a, SharpRuntime::ulongcs b) {
            return (a < b) ? -1 : (a > b) ? 1 : 0;
        }

        /** @brief Returns true if the two values are equal. */
        static bool Equals(SharpRuntime::ulongcs a, SharpRuntime::ulongcs b) { return a == b; }

        /** @brief Returns a hash code for the value. */
        static intcs GetHashCode(SharpRuntime::ulongcs value) {
            return static_cast<intcs>(value ^ (value >> 32));
        }

        /** @brief Returns the larger of two UInt64 values. */
        static SharpRuntime::ulongcs Max(SharpRuntime::ulongcs x, SharpRuntime::ulongcs y) {
            return std::max(x, y);
        }

        /** @brief Returns the smaller of two UInt64 values. */
        static SharpRuntime::ulongcs Min(SharpRuntime::ulongcs x, SharpRuntime::ulongcs y) {
            return std::min(x, y);
        }

        /** @brief Clamps value to the inclusive range [min, max]. */
        static SharpRuntime::ulongcs Clamp(SharpRuntime::ulongcs value,
                                           SharpRuntime::ulongcs min,
                                           SharpRuntime::ulongcs max) {
            return std::clamp(value, min, max);
        }

        /** @brief Returns 0 if value is zero; 1 otherwise. */
        static intcs Sign(SharpRuntime::ulongcs value) { return value == 0 ? 0 : 1; }

        /** @brief Divides left by right and returns a (quotient, remainder) pair. */
        static std::pair<SharpRuntime::ulongcs, SharpRuntime::ulongcs>
        DivRem(SharpRuntime::ulongcs left, SharpRuntime::ulongcs right) {
            return { left / right, left % right };
        }

        /** @brief Returns true if value is an even integer. */
        static bool IsEvenInteger(SharpRuntime::ulongcs value) { return (value & 1) == 0; }

        /** @brief Returns true if value is an odd integer. */
        static bool IsOddInteger(SharpRuntime::ulongcs value) { return (value & 1) != 0; }

        /** @brief Returns true if value is a power of two. */
        static bool IsPow2(SharpRuntime::ulongcs value) {
            return value != 0 && (value & (value - 1)) == 0;
        }

        /**
         * @brief Returns the floor log base 2 of value.
         * Matches .NET's explicit "0 -> 0" contract: Log2(0) returns 0, it does not throw
         * or wrap around (bit_width(0) - 1 would otherwise underflow to UINT64_MAX).
         */
        static SharpRuntime::ulongcs Log2(SharpRuntime::ulongcs value) {
            if (value == 0) return 0;
            return static_cast<uint64_t>(std::bit_width(value) - 1);
        }

        /** @brief Returns the number of leading zero bits in the 64-bit representation. */
        static SharpRuntime::ulongcs LeadingZeroCount(SharpRuntime::ulongcs value) {
            return static_cast<uint64_t>(std::countl_zero(value));
        }

        /** @brief Returns the number of trailing zero bits. */
        static SharpRuntime::ulongcs TrailingZeroCount(SharpRuntime::ulongcs value) {
            if (value == 0) return 64;
            return static_cast<uint64_t>(std::countr_zero(value));
        }

        /** @brief Returns the number of set bits (population count). */
        static SharpRuntime::ulongcs PopCount(SharpRuntime::ulongcs value) {
            return static_cast<uint64_t>(std::popcount(value));
        }

        /** @brief Rotates value left by rotateAmount bits. */
        static SharpRuntime::ulongcs RotateLeft(SharpRuntime::ulongcs value, intcs rotateAmount) {
            return std::rotl(value, rotateAmount);
        }

        /** @brief Rotates value right by rotateAmount bits. */
        static SharpRuntime::ulongcs RotateRight(SharpRuntime::ulongcs value, intcs rotateAmount) {
            return std::rotr(value, rotateAmount);
        }
    };

} // namespace System
