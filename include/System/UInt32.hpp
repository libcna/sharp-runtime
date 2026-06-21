// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::uintcs;

    /**
     * @brief Represents a 32-bit unsigned integer.
     *
     * C++ counterpart of .NET System.UInt32.
     * All members are static; the class cannot be instantiated.
     * The underlying C++ type is @c uint32_t (aliased as @c SharpRuntime::uintcs).
     */
    class UInt32 {
    public:
        UInt32() = delete;

        /** @brief The maximum value of a UInt32 (4 294 967 295). C++ counterpart of .NET UInt32.MaxValue. */
        static constexpr uintcs MaxValue = std::numeric_limits<uint32_t>::max();

        /** @brief The minimum value of a UInt32 (0). C++ counterpart of .NET UInt32.MinValue. */
        static constexpr uintcs MinValue = 0;

        /**
         * @brief Parses @p s as a decimal UInt32 value.
         * C++ counterpart of .NET UInt32.Parse(string).
         * @throws std::invalid_argument on bad format or overflow.
         */
        [[nodiscard]] static uintcs Parse(const std::string& s) {
            try {
                unsigned long v = std::stoul(s);
                if (v > MaxValue) throw std::out_of_range("overflow");
                return static_cast<uint32_t>(v);
            }
            catch (const std::invalid_argument&) {
                throw std::invalid_argument("Input string was not in a correct format.");
            }
            catch (const std::out_of_range&) {
                throw std::invalid_argument("Input string was not in a correct format.");
            }
        }

        /**
         * @brief Attempts to parse @p s as a UInt32; returns false on failure.
         * C++ counterpart of .NET UInt32.TryParse(string, out uint).
         */
        static bool TryParse(const std::string& s, uintcs& result) noexcept {
            try { result = Parse(s); return true; }
            catch (...) { result = 0; return false; }
        }

        /**
         * @brief Converts @p value to its decimal string representation.
         * C++ counterpart of .NET UInt32.ToString().
         */
        [[nodiscard]] static std::string ToString(uintcs value) { return std::to_string(value); }

        /**
         * @brief Compares @p a to @p b and returns a signed integer.
         * C++ counterpart of .NET UInt32.CompareTo(uint).
         */
        [[nodiscard]] static int CompareTo(uintcs a, uintcs b) noexcept {
            return (a < b) ? -1 : (a > b) ? 1 : 0;
        }

        /** @brief Returns true if @p a equals @p b. C++ counterpart of .NET UInt32.Equals(uint). */
        [[nodiscard]] static bool Equals(uintcs a, uintcs b) noexcept { return a == b; }

        /** @brief Returns a hash code for @p value. C++ counterpart of .NET UInt32.GetHashCode(). */
        [[nodiscard]] static int GetHashCode(uintcs value) noexcept { return static_cast<int>(value); }

        /** @brief Clamps @p value to [@p min, @p max]. C++ counterpart of .NET UInt32.Clamp(uint,uint,uint). */
        [[nodiscard]] static uintcs Clamp(uintcs value, uintcs min, uintcs max) noexcept {
            return std::clamp(value, min, max);
        }

        /** @brief Returns the larger of @p x and @p y. C++ counterpart of .NET UInt32.Max(uint,uint). */
        [[nodiscard]] static uintcs Max(uintcs x, uintcs y) noexcept { return x > y ? x : y; }

        /** @brief Returns the smaller of @p x and @p y. C++ counterpart of .NET UInt32.Min(uint,uint). */
        [[nodiscard]] static uintcs Min(uintcs x, uintcs y) noexcept { return x < y ? x : y; }

        /** @brief Returns 0 if @p value is zero; otherwise 1. C++ counterpart of .NET UInt32.Sign(uint). */
        [[nodiscard]] static int Sign(uintcs value) noexcept { return value == 0 ? 0 : 1; }

        /**
         * @brief Returns the quotient and remainder of @p left / @p right.
         * C++ counterpart of .NET UInt32.DivRem(uint,uint).
         */
        [[nodiscard]] static std::pair<uintcs, uintcs> DivRem(uintcs left, uintcs right) {
            return {left / right, left % right};
        }

        /** @brief Returns true when @p value is even. C++ counterpart of .NET UInt32.IsEvenInteger(uint). */
        [[nodiscard]] static bool IsEvenInteger(uintcs value) noexcept { return (value & 1) == 0; }

        /** @brief Returns true when @p value is odd. C++ counterpart of .NET UInt32.IsOddInteger(uint). */
        [[nodiscard]] static bool IsOddInteger(uintcs value) noexcept { return (value & 1) != 0; }

        /** @brief Returns true when @p value is a power of two. C++ counterpart of .NET UInt32.IsPow2(uint). */
        [[nodiscard]] static bool IsPow2(uintcs value) noexcept {
            return value != 0 && (value & (value - 1)) == 0;
        }

        /** @brief Returns the number of leading zero bits. C++ counterpart of .NET UInt32.LeadingZeroCount(uint). */
        [[nodiscard]] static uintcs LeadingZeroCount(uintcs value) noexcept {
            return static_cast<uintcs>(std::countl_zero(value));
        }

        /** @brief Returns the number of set bits. C++ counterpart of .NET UInt32.PopCount(uint). */
        [[nodiscard]] static uintcs PopCount(uintcs value) noexcept {
            return static_cast<uintcs>(std::popcount(value));
        }

        /** @brief Returns the number of trailing zero bits. C++ counterpart of .NET UInt32.TrailingZeroCount(uint). */
        [[nodiscard]] static uintcs TrailingZeroCount(uintcs value) noexcept {
            if (value == 0) return 32;
            return static_cast<uintcs>(std::countr_zero(value));
        }

        /** @brief Rotates @p value left by @p rotateAmount bits. C++ counterpart of .NET UInt32.RotateLeft(uint,int). */
        [[nodiscard]] static uintcs RotateLeft(uintcs value, int rotateAmount) noexcept {
            return std::rotl(value, rotateAmount);
        }

        /** @brief Rotates @p value right by @p rotateAmount bits. C++ counterpart of .NET UInt32.RotateRight(uint,int). */
        [[nodiscard]] static uintcs RotateRight(uintcs value, int rotateAmount) noexcept {
            return std::rotr(value, rotateAmount);
        }

        /**
         * @brief Returns the floor of the base-2 logarithm of @p value.
         * C++ counterpart of .NET UInt32.Log2(uint).
         * @throws std::domain_error if @p value is 0.
         */
        [[nodiscard]] static uintcs Log2(uintcs value) {
            if (value == 0) throw std::domain_error("Log2 of zero is undefined.");
            return static_cast<uintcs>(std::bit_width(value) - 1);
        }
    };

} // namespace System
