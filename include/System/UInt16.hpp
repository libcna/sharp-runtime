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
#include <string>
#include <utility>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/FormatException.hpp"
#include "System/OverflowException.hpp"

namespace System {

    using SharpRuntime::intcs;

    /**
     * @brief Provides static helper methods that mirror .NET System.UInt16.
     *
     * Represents a 16-bit unsigned integer (0 to 65535).
     */
    class UInt16 {
    public:
        /** @brief Maximum value of a 16-bit unsigned integer (65535). */
        static constexpr SharpRuntime::ushortcs MaxValue = std::numeric_limits<uint16_t>::max();
        /** @brief Minimum value of a 16-bit unsigned integer (0). */
        static constexpr SharpRuntime::ushortcs MinValue = 0;

        /**
         * @brief Converts the string representation of a number to its UInt16 equivalent.
         * @throws System::OverflowException if the value exceeds UInt16 range.
         * @throws System::FormatException if the string is not a valid integer.
         */
        static SharpRuntime::ushortcs Parse(const std::string& s) {
            try {
                unsigned long v = std::stoul(s);
                if (v > MaxValue) throw System::OverflowException("Value was either too large or too small for a UInt16.");
                return static_cast<uint16_t>(v);
            } catch (const System::OverflowException&) { throw; }
              catch (const std::out_of_range&) { throw System::OverflowException("Value was either too large or too small for a UInt16."); }
              catch (...) { throw System::FormatException("Input string was not in a correct format."); }
        }

        /**
         * @brief Tries to convert a string to a UInt16 without throwing.
         * @param result Receives the parsed value on success, or 0 on failure.
         * @return true if parsing succeeded; false otherwise.
         */
        static bool TryParse(const std::string& s, SharpRuntime::ushortcs& result) {
            try { result = Parse(s); return true; }
            catch (...) { result = 0; return false; }
        }

        /** @brief Converts the value to its decimal string representation. */
        static std::string ToString(SharpRuntime::ushortcs value) { return std::to_string(value); }

        /** @brief Converts value to a string using a format specifier ("X", "X4", "D", "D5", "G"). */
        static std::string ToString(SharpRuntime::ushortcs value, const std::string& format) {
            if (format.empty()) return ToString(value);
            char type = format[0];
            int width = format.size() > 1 ? std::stoi(format.substr(1)) : 0;
            unsigned uv = static_cast<unsigned>(value);
            std::ostringstream oss;
            if (type == 'X') { oss << std::uppercase << std::hex << std::setfill('0') << std::setw(width) << uv; return oss.str(); }
            if (type == 'x') { oss << std::hex << std::setfill('0') << std::setw(width) << uv; return oss.str(); }
            if (type == 'D' || type == 'd') {
                std::string s = std::to_string(uv);
                while (static_cast<int>(s.size()) < width) s = "0" + s;
                return s;
            }
            if (type == 'G' || type == 'g') return ToString(value);
            return ToString(value);
        }

        /** @brief Compares this value to another UInt16. Returns negative, zero, or positive. */
        static intcs CompareTo(SharpRuntime::ushortcs a, SharpRuntime::ushortcs b) {
            return (a < b) ? -1 : (a > b) ? 1 : 0;
        }

        /** @brief Returns true if the two values are equal. */
        static bool Equals(SharpRuntime::ushortcs a, SharpRuntime::ushortcs b) { return a == b; }

        /** @brief Returns a hash code for the value. */
        static intcs GetHashCode(SharpRuntime::ushortcs value) { return static_cast<intcs>(value); }

        /** @brief Returns the larger of two UInt16 values. */
        static SharpRuntime::ushortcs Max(SharpRuntime::ushortcs x, SharpRuntime::ushortcs y) {
            return std::max(x, y);
        }

        /** @brief Returns the smaller of two UInt16 values. */
        static SharpRuntime::ushortcs Min(SharpRuntime::ushortcs x, SharpRuntime::ushortcs y) {
            return std::min(x, y);
        }

        /** @brief Clamps value to the inclusive range [min, max]. */
        static SharpRuntime::ushortcs Clamp(SharpRuntime::ushortcs value,
                                            SharpRuntime::ushortcs min,
                                            SharpRuntime::ushortcs max) {
            return std::clamp(value, min, max);
        }

        /** @brief Returns 0 if value is zero; 1 otherwise. */
        static intcs Sign(SharpRuntime::ushortcs value) { return value == 0 ? 0 : 1; }

        /** @brief Divides left by right and returns a (quotient, remainder) pair. */
        static std::pair<SharpRuntime::ushortcs, SharpRuntime::ushortcs>
        DivRem(SharpRuntime::ushortcs left, SharpRuntime::ushortcs right) {
            return { static_cast<uint16_t>(left / right), static_cast<uint16_t>(left % right) };
        }

        /** @brief Returns true if value is an even integer. */
        static bool IsEvenInteger(SharpRuntime::ushortcs value) { return (value & 1) == 0; }

        /** @brief Returns true if value is an odd integer. */
        static bool IsOddInteger(SharpRuntime::ushortcs value) { return (value & 1) != 0; }

        /** @brief Returns true if value is a power of two. */
        static bool IsPow2(SharpRuntime::ushortcs value) {
            return value != 0 && (value & (value - 1)) == 0;
        }

        /**
         * @brief Returns the floor log base 2 of value.
         * Matches .NET's explicit "0 -> 0" contract: Log2(0) returns 0, it does not throw
         * or wrap around (bit_width(0) - 1 would otherwise underflow to 65535).
         */
        static SharpRuntime::ushortcs Log2(SharpRuntime::ushortcs value) {
            if (value == 0) return 0;
            return static_cast<uint16_t>(std::bit_width(static_cast<unsigned>(value)) - 1);
        }

        /** @brief Returns the number of leading zero bits in the 16-bit representation. */
        static SharpRuntime::ushortcs LeadingZeroCount(SharpRuntime::ushortcs value) {
            return static_cast<uint16_t>(std::countl_zero(static_cast<uint32_t>(value)) - 16);
        }

        /** @brief Returns the number of trailing zero bits. */
        static SharpRuntime::ushortcs TrailingZeroCount(SharpRuntime::ushortcs value) {
            if (value == 0) return 16;
            return static_cast<uint16_t>(std::countr_zero(static_cast<uint32_t>(value)));
        }

        /** @brief Returns the number of set bits (population count). */
        static SharpRuntime::ushortcs PopCount(SharpRuntime::ushortcs value) {
            return static_cast<uint16_t>(std::popcount(static_cast<uint32_t>(value)));
        }

        /** @brief Rotates value left by rotateAmount bits within a 16-bit field. */
        static SharpRuntime::ushortcs RotateLeft(SharpRuntime::ushortcs value, intcs rotateAmount) {
            rotateAmount &= 15;
            return static_cast<uint16_t>((value << rotateAmount) | (value >> (16 - rotateAmount)));
        }

        /** @brief Rotates value right by rotateAmount bits within a 16-bit field. */
        static SharpRuntime::ushortcs RotateRight(SharpRuntime::ushortcs value, intcs rotateAmount) {
            rotateAmount &= 15;
            return static_cast<uint16_t>((value >> rotateAmount) | (value << (16 - rotateAmount)));
        }
    };

} // namespace System
