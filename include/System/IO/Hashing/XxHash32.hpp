// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include "System/IO/Hashing/NonCryptographicHashAlgorithm.hpp"

namespace System::IO::Hashing {

    // xxHash32 — fast non-cryptographic hash (xxHash by Yann Collet)
    class XxHash32 : public NonCryptographicHashAlgorithm {
        static constexpr uint32_t Prime1 = 0x9E3779B1u;
        static constexpr uint32_t Prime2 = 0x85EBCA77u;
        static constexpr uint32_t Prime3 = 0xC2B2AE3Du;
        static constexpr uint32_t Prime4 = 0x27D4EB2Fu;
        static constexpr uint32_t Prime5 = 0x165667B1u;

        uint32_t seed_;
        uint32_t v1_, v2_, v3_, v4_;
        uint64_t totalLength_ = 0;
        uint8_t  buf_[16] = {};
        int      bufLen_  = 0;

        static uint32_t rotl32(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

        void initState() {
            v1_ = seed_ + Prime1 + Prime2;
            v2_ = seed_ + Prime2;
            v3_ = seed_;
            v4_ = seed_ - Prime1;
        }

        static uint32_t round(uint32_t acc, uint32_t input) {
            acc += input * Prime2;
            acc = rotl32(acc, 13);
            acc *= Prime1;
            return acc;
        }

    public:
        explicit XxHash32(uint32_t seed = 0) : NonCryptographicHashAlgorithm(4), seed_(seed) {
            initState();
        }

        void Reset() override { totalLength_ = 0; bufLen_ = 0; initState(); }

        void Append(const uint8_t* source, size_t length) override {
            totalLength_ += length;
            const uint8_t* p = source;
            const uint8_t* end = source + length;

            if (bufLen_ > 0) {
                int need = 16 - bufLen_;
                int copy = static_cast<int>(length < static_cast<size_t>(need) ? length : static_cast<size_t>(need));
                std::memcpy(buf_ + bufLen_, p, static_cast<size_t>(copy));
                bufLen_ += copy;
                p += copy;
                if (bufLen_ < 16) return;
                processBlock(buf_);
                bufLen_ = 0;
            }

            while (p + 16 <= end) {
                processBlock(p);
                p += 16;
            }

            bufLen_ = static_cast<int>(end - p);
            if (bufLen_ > 0) std::memcpy(buf_, p, static_cast<size_t>(bufLen_));
        }

        void GetCurrentHash(uint8_t* dest, size_t /*len*/) override {
            uint32_t h32;
            if (totalLength_ >= 16) {
                h32 = rotl32(v1_, 1) + rotl32(v2_, 7) + rotl32(v3_, 12) + rotl32(v4_, 18);
            } else {
                h32 = seed_ + Prime5;
            }
            h32 += static_cast<uint32_t>(totalLength_);

            const uint8_t* p = buf_;
            int remaining = bufLen_;
            while (remaining >= 4) {
                uint32_t lane; std::memcpy(&lane, p, 4);
                h32 += lane * Prime3;
                h32 = rotl32(h32, 17) * Prime4;
                p += 4; remaining -= 4;
            }
            while (remaining > 0) {
                h32 += (*p) * Prime5;
                h32 = rotl32(h32, 11) * Prime1;
                ++p; --remaining;
            }

            h32 ^= h32 >> 15; h32 *= Prime2;
            h32 ^= h32 >> 13; h32 *= Prime3;
            h32 ^= h32 >> 16;

            dest[0] = static_cast<uint8_t>(h32 & 0xFF);
            dest[1] = static_cast<uint8_t>((h32 >> 8) & 0xFF);
            dest[2] = static_cast<uint8_t>((h32 >> 16) & 0xFF);
            dest[3] = static_cast<uint8_t>((h32 >> 24) & 0xFF);
        }

        [[nodiscard]] uint32_t GetCurrentHashAsUInt32() {
            uint8_t buf[4];
            GetCurrentHash(buf, 4);
            uint32_t r; std::memcpy(&r, buf, 4); return r;
        }

        static uint32_t HashToUInt32(const std::vector<uint8_t>& source, uint32_t seed = 0) {
            XxHash32 h(seed);
            h.Append(source);
            return h.GetCurrentHashAsUInt32();
        }

    private:
        void processBlock(const uint8_t* block) {
            uint32_t lane;
            std::memcpy(&lane, block,      4); v1_ = round(v1_, lane);
            std::memcpy(&lane, block + 4,  4); v2_ = round(v2_, lane);
            std::memcpy(&lane, block + 8,  4); v3_ = round(v3_, lane);
            std::memcpy(&lane, block + 12, 4); v4_ = round(v4_, lane);
        }
    };

} // namespace System::IO::Hashing
