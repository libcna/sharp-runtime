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
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/DivideByZeroException.hpp"
#include "System/FormatException.hpp"
#include "System/Int128.hpp"
#include "System/OverflowException.hpp"

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
         * @throws System::FormatException on bad format.
         * @throws System::OverflowException if the value exceeds Int64 range.
         */
        [[nodiscard]] static longcs Parse(const std::string& s) {
            std::size_t pos = 0;
            int64_t v;
            try {
                v = static_cast<int64_t>(std::stoll(s, &pos));
            } catch (const std::out_of_range&) {
                throw System::OverflowException("Value was either too large or too small for an Int64.");
            } catch (...) {
                throw System::FormatException("Input string was not in a correct format.");
            }
            for (; pos < s.size(); ++pos) {
                if (!std::isspace(static_cast<unsigned char>(s[pos])))
                    throw System::FormatException("Input string was not in a correct format.");
            }
            return v;
        }

        /**
         * @brief Attempts to parse @p s as an Int64; returns false on failure.
         * C++ counterpart of .NET Int64.TryParse(string, out long).
         */
        static bool TryParse(const std::string& s, longcs& result) noexcept {
            try { result = Parse(s); return true; }
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
            int width = 0;
            if (format.size() > 1) {
                try {
                    width = std::stoi(format.substr(1));
                } catch (const std::exception&) {
                    throw System::FormatException("Format specifier was invalid.");
                }
            }
            std::ostringstream oss;
            oss.imbue(std::locale::classic());
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

        /**
         * @brief Returns the absolute value of @p value. C++ counterpart of .NET Math.Abs(long).
         * @throws System::OverflowException if @p value is MinValue.
         */
        [[nodiscard]] static longcs Abs(longcs value) {
            if (value == MinValue) throw System::OverflowException("Negating the minimum value of a twos complement number is invalid.");
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
         * @throws System::DivideByZeroException if @p right is zero -- integer division
         *         by zero is undefined behavior in C++ (a hardware trap, not a catchable
         *         exception), unlike the CLR's div instruction which .NET surfaces as a
         *         managed DivideByZeroException; this must be checked explicitly.
         * @throws System::OverflowException if @p left is MinValue and @p right is -1
         *         (the mathematical result does not fit in Int64; the CLR's div
         *         instruction traps on this input and .NET surfaces it as
         *         OverflowException). int64_t division runs at native width with no
         *         wider promotion available, so this is real, reachable undefined
         *         behavior in C++ if left unchecked -- same bug class as Int32::DivRem.
         */
        [[nodiscard]] static std::pair<longcs, longcs> DivRem(longcs left, longcs right) {
            if (right == 0)
                throw System::DivideByZeroException();
            if (left == MinValue && right == -1)
                throw System::OverflowException("Negating the minimum value of a twos complement number is invalid.");
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
         * C++ counterpart of .NET Int64.Log2(long). Matches .NET: Log2(0) is 0, not an error
         * (BitOperations.Log2(0) is documented to return 0).
         * @throws System::ArgumentOutOfRangeException if @p value is negative.
         */
        [[nodiscard]] static int Log2(longcs value) {
            if (value < 0) throw System::ArgumentOutOfRangeException("value", "value must be non-negative");
            if (value == 0) return 0;
            return std::bit_width(static_cast<uint64_t>(value)) - 1;
        }

        /**
         * @brief Produces the full 128-bit product of two 64-bit numbers.
         * C++ counterpart of .NET Int64.BigMul(long, long).
         */
        [[nodiscard]] static Int128 BigMul(longcs left, longcs right) noexcept {
            __int128 product = static_cast<__int128>(left) * static_cast<__int128>(right);
            return Int128(static_cast<uint64_t>(static_cast<unsigned __int128>(product) >> 64),
                          static_cast<uint64_t>(product));
        }

        /**
         * @brief Copies the sign of @p sign to the magnitude of @p value.
         * C++ counterpart of .NET Int64.CopySign(long, long).
         * @throws System::OverflowException if @p value is MinValue and @p sign is non-negative
         *         (its magnitude does not fit in a signed Int64).
         */
        [[nodiscard]] static longcs CopySign(longcs value, longcs sign) {
            longcs absValue = value < 0 ? (value == MinValue ? value : -value) : value;
            if (sign >= 0) {
                if (absValue < 0) throw System::OverflowException("Negating the minimum value of a twos complement number is invalid.");
                return absValue;
            }
            return -absValue;
        }

        /**
         * @brief Returns the value with the larger absolute value.
         * C++ counterpart of .NET Int64.MaxMagnitude(long, long). MinValue has no
         * representable positive magnitude, so it always wins, matching .NET.
         */
        [[nodiscard]] static longcs MaxMagnitude(longcs x, longcs y) noexcept {
            if (x == MinValue) return x;
            if (y == MinValue) return y;
            longcs ax = x < 0 ? -x : x;
            longcs ay = y < 0 ? -y : y;
            if (ax > ay) return x;
            if (ax == ay) return IsNegative(x) ? y : x;
            return y;
        }

        /**
         * @brief Returns the value with the smaller absolute value.
         * C++ counterpart of .NET Int64.MinMagnitude(long, long). MinValue has no
         * representable positive magnitude, so it always loses, matching .NET.
         */
        [[nodiscard]] static longcs MinMagnitude(longcs x, longcs y) noexcept {
            if (x == MinValue) return y;
            if (y == MinValue) return x;
            longcs ax = x < 0 ? -x : x;
            longcs ay = y < 0 ? -y : y;
            if (ax < ay) return x;
            if (ax == ay) return IsNegative(x) ? x : y;
            return y;
        }

        /** @brief Returns true if @p value is negative. C++ counterpart of .NET Int64.IsNegative(long). */
        [[nodiscard]] static bool IsNegative(longcs value) noexcept { return value < 0; }

        /** @brief Returns true if @p value is zero or positive. C++ counterpart of .NET Int64.IsPositive(long). */
        [[nodiscard]] static bool IsPositive(longcs value) noexcept { return value >= 0; }
    };

} // namespace System

