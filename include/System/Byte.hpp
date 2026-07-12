// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/DivideByZeroException.hpp"
#include "System/FormatException.hpp"
#include "System/OverflowException.hpp"

namespace System {

    using SharpRuntime::bytecs;

    /**
     * @brief Represents an 8-bit unsigned integer.
     *
     * C++ counterpart of .NET System.Byte.
     * All members are static; the class cannot be instantiated.
     * The underlying C++ type is @c uint8_t (aliased as @c SharpRuntime::bytecs).
     */
    class Byte {
    public:
        Byte() = delete;

        // -----------------------------------------------------------------------
        // Constants
        // -----------------------------------------------------------------------

        /** @brief The maximum value of a Byte (255). C++ counterpart of .NET Byte.MaxValue. */
        static constexpr bytecs MaxValue = std::numeric_limits<uint8_t>::max();

        /** @brief The minimum value of a Byte (0). C++ counterpart of .NET Byte.MinValue. */
        static constexpr bytecs MinValue = 0;

        // -----------------------------------------------------------------------
        // Comparison
        // -----------------------------------------------------------------------

        /**
         * @brief Compares @p a to @p b and returns a signed integer.
         *
         * C++ counterpart of .NET Byte.CompareTo(byte).
         * @return Negative if a < b, zero if equal, positive if a > b.
         */
        [[nodiscard]] static int CompareTo(bytecs a, bytecs b) noexcept {
            return static_cast<int>(a) - static_cast<int>(b);
        }

        /**
         * @brief Returns true if @p a equals @p b.
         *
         * C++ counterpart of .NET Byte.Equals(byte).
         */
        [[nodiscard]] static bool Equals(bytecs a, bytecs b) noexcept { return a == b; }

        // -----------------------------------------------------------------------
        // Hash
        // -----------------------------------------------------------------------

        /**
         * @brief Returns a hash code for @p value.
         *
         * C++ counterpart of .NET Byte.GetHashCode().
         */
        [[nodiscard]] static int GetHashCode(bytecs value) noexcept {
            return static_cast<int>(value);
        }

        // -----------------------------------------------------------------------
        // Arithmetic helpers
        // -----------------------------------------------------------------------

        /**
         * @brief Clamps @p value to the inclusive range [@p min, @p max].
         *
         * C++ counterpart of .NET Byte.Clamp(byte, byte, byte).
         */
        [[nodiscard]] static bytecs Clamp(bytecs value, bytecs min, bytecs max) noexcept {
            return std::clamp(value, min, max);
        }

        /**
         * @brief Returns the larger of @p x and @p y.
         *
         * C++ counterpart of .NET Byte.Max(byte, byte).
         */
        [[nodiscard]] static bytecs Max(bytecs x, bytecs y) noexcept {
            return x > y ? x : y;
        }

        /**
         * @brief Returns the smaller of @p x and @p y.
         *
         * C++ counterpart of .NET Byte.Min(byte, byte).
         */
        [[nodiscard]] static bytecs Min(bytecs x, bytecs y) noexcept {
            return x < y ? x : y;
        }

        /**
         * @brief Returns 0 if @p value is zero; otherwise 1.
         *
         * C++ counterpart of .NET Byte.Sign(byte).
         * Bytes are unsigned so the result is never -1.
         */
        [[nodiscard]] static int Sign(bytecs value) noexcept {
            return value == 0 ? 0 : 1;
        }

        /**
         * @brief Returns the quotient and remainder of @p left / @p right.
         *
         * C++ counterpart of .NET Byte.DivRem(byte, byte).
         * @throws System::DivideByZeroException if @p right is zero -- integer division
         *         by zero is undefined behavior in C++ (a hardware trap, not a catchable
         *         exception), unlike the CLR's div instruction which .NET surfaces as a
         *         managed DivideByZeroException; this must be checked explicitly.
         */
        [[nodiscard]] static std::pair<bytecs, bytecs> DivRem(bytecs left, bytecs right) {
            if (right == 0)
                throw System::DivideByZeroException();
            return {static_cast<bytecs>(left / right),
                    static_cast<bytecs>(left % right)};
        }

        // -----------------------------------------------------------------------
        // Bit predicates
        // -----------------------------------------------------------------------

        /**
         * @brief Returns true when @p value is even.
         *
         * C++ counterpart of .NET Byte.IsEvenInteger(byte).
         */
        [[nodiscard]] static bool IsEvenInteger(bytecs value) noexcept {
            return (value & 1) == 0;
        }

        /**
         * @brief Returns true when @p value is odd.
         *
         * C++ counterpart of .NET Byte.IsOddInteger(byte).
         */
        [[nodiscard]] static bool IsOddInteger(bytecs value) noexcept {
            return (value & 1) != 0;
        }

        /**
         * @brief Returns true when @p value is a power of two.
         *
         * C++ counterpart of .NET Byte.IsPow2(byte).
         */
        [[nodiscard]] static bool IsPow2(bytecs value) noexcept {
            return value != 0 && (value & (value - 1)) == 0;
        }

        // -----------------------------------------------------------------------
        // Bit operations
        // -----------------------------------------------------------------------

        /**
         * @brief Returns the number of leading zero bits in @p value.
         *
         * C++ counterpart of .NET Byte.LeadingZeroCount(byte).
         */
        [[nodiscard]] static bytecs LeadingZeroCount(bytecs value) noexcept {
            if (value == 0) return 8;
            return static_cast<bytecs>(std::countl_zero(static_cast<uint32_t>(value)) - 24);
        }

        /**
         * @brief Returns the number of set bits in @p value.
         *
         * C++ counterpart of .NET Byte.PopCount(byte).
         */
        [[nodiscard]] static bytecs PopCount(bytecs value) noexcept {
            return static_cast<bytecs>(std::popcount(static_cast<uint32_t>(value)));
        }

        /**
         * @brief Returns the number of trailing zero bits in @p value.
         *
         * C++ counterpart of .NET Byte.TrailingZeroCount(byte).
         */
        [[nodiscard]] static bytecs TrailingZeroCount(bytecs value) noexcept {
            if (value == 0) return 8;
            return static_cast<bytecs>(std::countr_zero(static_cast<uint32_t>(value)));
        }

        /**
         * @brief Rotates @p value left by @p rotateAmount bits.
         *
         * C++ counterpart of .NET Byte.RotateLeft(byte, int).
         */
        [[nodiscard]] static bytecs RotateLeft(bytecs value, int rotateAmount) noexcept {
            int r = rotateAmount & 7;
            return static_cast<bytecs>((value << r) | (value >> ((8 - r) & 7)));
        }

        /**
         * @brief Rotates @p value right by @p rotateAmount bits.
         *
         * C++ counterpart of .NET Byte.RotateRight(byte, int).
         */
        [[nodiscard]] static bytecs RotateRight(bytecs value, int rotateAmount) noexcept {
            int r = rotateAmount & 7;
            return static_cast<bytecs>((value >> r) | (value << ((8 - r) & 7)));
        }

        /**
         * @brief Returns the floor of the base-2 logarithm of @p value.
         *
         * C++ counterpart of .NET Byte.Log2(byte), which delegates to
         * BitOperations.Log2 — by convention Log2(0) returns 0 rather than throwing,
         * since byte can never be negative and there is nothing else to signal.
         */
        [[nodiscard]] static bytecs Log2(bytecs value) noexcept {
            if (value == 0) return 0;
            return static_cast<bytecs>(std::bit_width(static_cast<uint32_t>(value)) - 1);
        }

        /**
         * @brief Returns the base-10 logarithm of @p value, truncated to Byte.
         *
         * C++ counterpart of .NET Byte.Log10(byte), which delegates to uint.Log10 —
         * by convention Log10(0) returns 0 rather than throwing.
         */
        [[nodiscard]] static bytecs Log10(bytecs value) noexcept {
            bytecs result = 0;
            unsigned v = static_cast<unsigned>(value);
            while (v >= 10) { v /= 10; ++result; }
            return result;
        }

        // -----------------------------------------------------------------------
        // Parse / TryParse / ToString
        // -----------------------------------------------------------------------

        /**
         * @brief Parses @p s as a decimal Byte value.
         *
         * C++ counterpart of .NET Byte.Parse(string) with NumberStyles.Integer:
         * leading/trailing whitespace and a leading sign are tolerated, but any
         * trailing non-whitespace character is rejected.
         * @throws System::FormatException on bad format.
         * @throws System::OverflowException if the value is outside [0, 255].
         */
        [[nodiscard]] static bytecs Parse(const std::string& s) {
            std::size_t pos = 0;
            int v;
            try {
                v = std::stoi(s, &pos);
            } catch (const std::out_of_range&) {
                throw System::OverflowException("Value was either too large or too small for an unsigned byte.");
            } catch (...) {
                throw System::FormatException("Input string was not in a correct format.");
            }
            // Reject trailing non-whitespace (.NET NumberStyles.Integer parity).
            for (; pos < s.size(); ++pos) {
                if (!std::isspace(static_cast<unsigned char>(s[pos])))
                    throw System::FormatException("Input string was not in a correct format.");
            }
            if (v < 0 || v > MaxValue)
                throw System::OverflowException("Value was either too large or too small for an unsigned byte.");
            return static_cast<bytecs>(v);
        }

        /**
         * @brief Attempts to parse @p s as a Byte; returns false on failure.
         *
         * C++ counterpart of .NET Byte.TryParse(string, out byte).
         */
        static bool TryParse(const std::string& s, bytecs& result) noexcept {
            try { result = Parse(s); return true; }
            catch (...) { result = 0; return false; }
        }

        /**
         * @brief Converts @p value to its decimal string representation.
         *
         * C++ counterpart of .NET Byte.ToString().
         */
        [[nodiscard]] static std::string ToString(bytecs value) {
            return std::to_string(static_cast<unsigned>(value));
        }

        /**
         * @brief Converts @p value to a string using the specified format specifier.
         *
         * C++ counterpart of .NET Byte.ToString(string).
         * Supported formats: "X"/"x" (hex), "D"/"d" (decimal with width),
         * "G"/"g" (general), "B"/"b" (binary).
         */
        [[nodiscard]] static std::string ToString(bytecs value, const std::string& format) {
            if (format.empty()) return ToString(value);
            char type = format[0];
            int  width = format.size() > 1 ? std::stoi(format.substr(1)) : 0;
            unsigned uv = static_cast<unsigned>(value);
            std::ostringstream oss;
            oss.imbue(std::locale::classic());
            if (type == 'X') {
                oss << std::uppercase << std::hex
                    << std::setfill('0') << std::setw(width) << uv;
                return oss.str();
            }
            if (type == 'x') {
                oss << std::hex << std::setfill('0') << std::setw(width) << uv;
                return oss.str();
            }
            if (type == 'D' || type == 'd') {
                std::string s = std::to_string(uv);
                while (static_cast<int>(s.size()) < width) s = "0" + s;
                return s;
            }
            if (type == 'G' || type == 'g') return ToString(value);
            if (type == 'B' || type == 'b') {
                std::string s;
                for (int i = 7; i >= 0; --i) s += ((uv >> i) & 1) ? '1' : '0';
                s.erase(0, s.find_first_not_of('0'));
                if (s.empty()) s = "0";
                while (static_cast<int>(s.size()) < width) s = "0" + s;
                return s;
            }
            return ToString(value);
        }
    };

} // namespace System
