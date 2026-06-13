// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <bit>
#include <cstdint>

namespace System::Numerics {

    class BitOperations {
    public:
        BitOperations() = delete;

        static bool IsPow2(uint32_t value) noexcept { return value != 0 && (value & (value - 1)) == 0; }
        static bool IsPow2(uint64_t value) noexcept { return value != 0 && (value & (value - 1)) == 0; }
        static bool IsPow2(int32_t value)  noexcept { return value > 0  && (value & (value - 1)) == 0; }
        static bool IsPow2(int64_t value)  noexcept { return value > 0  && (value & (value - 1)) == 0; }

        static uint32_t RoundUpToPowerOf2(uint32_t value) noexcept {
            if (value == 0) return 1;
            --value;
            value |= value >> 1; value |= value >> 2; value |= value >> 4;
            value |= value >> 8; value |= value >> 16;
            return value + 1;
        }
        static uint64_t RoundUpToPowerOf2(uint64_t value) noexcept {
            if (value == 0) return 1;
            --value;
            value |= value >> 1;  value |= value >> 2;  value |= value >> 4;
            value |= value >> 8;  value |= value >> 16; value |= value >> 32;
            return value + 1;
        }

        static int LeadingZeroCount(uint32_t value) noexcept {
            return static_cast<int>(std::countl_zero(value));
        }
        static int LeadingZeroCount(uint64_t value) noexcept {
            return static_cast<int>(std::countl_zero(value));
        }

        static int Log2(uint32_t value) noexcept {
            return value <= 1 ? 0 : static_cast<int>(std::bit_width(value)) - 1;
        }
        static int Log2(uint64_t value) noexcept {
            return value <= 1 ? 0 : static_cast<int>(std::bit_width(value)) - 1;
        }

        static int PopCount(uint32_t value) noexcept { return static_cast<int>(std::popcount(value)); }
        static int PopCount(uint64_t value) noexcept { return static_cast<int>(std::popcount(value)); }

        static int TrailingZeroCount(uint32_t value) noexcept {
            return static_cast<int>(std::countr_zero(value));
        }
        static int TrailingZeroCount(uint64_t value) noexcept {
            return static_cast<int>(std::countr_zero(value));
        }
        static int TrailingZeroCount(int32_t value) noexcept {
            return TrailingZeroCount(static_cast<uint32_t>(value));
        }

        static uint32_t RotateLeft(uint32_t value, int offset) noexcept {
            return std::rotl(value, offset);
        }
        static uint64_t RotateLeft(uint64_t value, int offset) noexcept {
            return std::rotl(value, offset);
        }
        static uint32_t RotateRight(uint32_t value, int offset) noexcept {
            return std::rotr(value, offset);
        }
        static uint64_t RotateRight(uint64_t value, int offset) noexcept {
            return std::rotr(value, offset);
        }

        static uint32_t ReverseBits(uint32_t value) noexcept {
            value = ((value >> 1) & 0x55555555u) | ((value & 0x55555555u) << 1);
            value = ((value >> 2) & 0x33333333u) | ((value & 0x33333333u) << 2);
            value = ((value >> 4) & 0x0F0F0F0Fu) | ((value & 0x0F0F0F0Fu) << 4);
            value = ((value >> 8) & 0x00FF00FFu) | ((value & 0x00FF00FFu) << 8);
            return (value >> 16) | (value << 16);
        }
    };

} // namespace System::Numerics
