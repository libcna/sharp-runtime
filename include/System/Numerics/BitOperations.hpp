// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <bit>
#include <cstdint>

namespace System::Numerics {

/**
 * Utility class providing static methods for common bit-manipulation operations.
 * 
 * All methods map directly to C++20 <bit> intrinsics (std::popcount, std::countl_zero,
 * std::rotl, std::rotr, std::bit_width) for maximum performance.
 */
class BitOperations {
public:
    BitOperations() = delete;

    /** @return True if @p value is an exact power of two (and non-zero). */
    static bool IsPow2(uint32_t value) noexcept { return value != 0 && (value & (value - 1)) == 0; }
    /** @return True if @p value is an exact power of two (and non-zero). */
    static bool IsPow2(uint64_t value) noexcept { return value != 0 && (value & (value - 1)) == 0; }
    /** @return True if @p value is a positive exact power of two. */
    static bool IsPow2(int32_t value)  noexcept { return value > 0  && (value & (value - 1)) == 0; }
    /** @return True if @p value is a positive exact power of two. */
    static bool IsPow2(int64_t value)  noexcept { return value > 0  && (value & (value - 1)) == 0; }

    /** @return The smallest power of two that is >= @p value; returns 1 for 0. */
    static uint32_t RoundUpToPowerOf2(uint32_t value) noexcept {
        if (value == 0) return 1;
        --value;
        value |= value >> 1; value |= value >> 2; value |= value >> 4;
        value |= value >> 8; value |= value >> 16;
        return value + 1;
    }
    /** @return The smallest power of two that is >= @p value; returns 1 for 0. */
    static uint64_t RoundUpToPowerOf2(uint64_t value) noexcept {
        if (value == 0) return 1;
        --value;
        value |= value >> 1;  value |= value >> 2;  value |= value >> 4;
        value |= value >> 8;  value |= value >> 16; value |= value >> 32;
        return value + 1;
    }

    /** @return The number of leading zero bits in @p value (0 to 32; 32 if value == 0). */
    static int LeadingZeroCount(uint32_t value) noexcept {
        return static_cast<int>(std::countl_zero(value));
    }
    /** @return The number of leading zero bits in @p value (0 to 64; 64 if value == 0). */
    static int LeadingZeroCount(uint64_t value) noexcept {
        return static_cast<int>(std::countl_zero(value));
    }

    /** @return Floor of log₂(value); returns 0 for value <= 1. */
    static int Log2(uint32_t value) noexcept {
        return value <= 1 ? 0 : static_cast<int>(std::bit_width(value)) - 1;
    }
    /** @return Floor of log₂(value); returns 0 for value <= 1. */
    static int Log2(uint64_t value) noexcept {
        return value <= 1 ? 0 : static_cast<int>(std::bit_width(value)) - 1;
    }

    /** @return The number of set bits (1-bits) in @p value. */
    static int PopCount(uint32_t value) noexcept { return static_cast<int>(std::popcount(value)); }
    /** @return The number of set bits (1-bits) in @p value. */
    static int PopCount(uint64_t value) noexcept { return static_cast<int>(std::popcount(value)); }

    /** @return The number of trailing zero bits in @p value (0 to 32; 32 if value == 0). */
    static int TrailingZeroCount(uint32_t value) noexcept {
        return static_cast<int>(std::countr_zero(value));
    }
    /** @return The number of trailing zero bits in @p value (0 to 64; 64 if value == 0). */
    static int TrailingZeroCount(uint64_t value) noexcept {
        return static_cast<int>(std::countr_zero(value));
    }
    /** @return The number of trailing zero bits in @p value (treats as unsigned). */
    static int TrailingZeroCount(int32_t value) noexcept {
        return TrailingZeroCount(static_cast<uint32_t>(value));
    }

    /** @return @p value rotated left by @p offset bits (wraps around at 32 bits). */
    static uint32_t RotateLeft(uint32_t value, int offset) noexcept {
        return std::rotl(value, offset);
    }
    /** @return @p value rotated left by @p offset bits (wraps around at 64 bits). */
    static uint64_t RotateLeft(uint64_t value, int offset) noexcept {
        return std::rotl(value, offset);
    }
    /** @return @p value rotated right by @p offset bits (wraps around at 32 bits). */
    static uint32_t RotateRight(uint32_t value, int offset) noexcept {
        return std::rotr(value, offset);
    }
    /** @return @p value rotated right by @p offset bits (wraps around at 64 bits). */
    static uint64_t RotateRight(uint64_t value, int offset) noexcept {
        return std::rotr(value, offset);
    }

    /** @return @p value with its bits in reversed order (bit 0 ↔ bit 31). */
    static uint32_t ReverseBits(uint32_t value) noexcept {
        value = ((value >> 1) & 0x55555555u) | ((value & 0x55555555u) << 1);
        value = ((value >> 2) & 0x33333333u) | ((value & 0x33333333u) << 2);
        value = ((value >> 4) & 0x0F0F0F0Fu) | ((value & 0x0F0F0F0Fu) << 4);
        value = ((value >> 8) & 0x00FF00FFu) | ((value & 0x00FF00FFu) << 8);
        return (value >> 16) | (value << 16);
    }
};

} // namespace System::Numerics
