// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include "System/IO/Hashing/NonCryptographicHashAlgorithm.hpp"

namespace System::IO::Hashing {

    // xxHash64 — fast non-cryptographic 64-bit hash (xxHash by Yann Collet)
    class XxHash64 : public NonCryptographicHashAlgorithm {
        static constexpr uint64_t Prime1 = 0x9E3779B185EBCA87ULL;
        static constexpr uint64_t Prime2 = 0xC2B2AE3D27D4EB4FULL;
        static constexpr uint64_t Prime3 = 0x165667B19E3779F9ULL;
        static constexpr uint64_t Prime4 = 0x85EBCA77C2B2AE63ULL;
        static constexpr uint64_t Prime5 = 0x27D4EB2F165667C5ULL;

        uint64_t seed_;
        uint64_t v1_, v2_, v3_, v4_;
        uint64_t totalLength_ = 0;
        uint8_t  buf_[32] = {};
        int      bufLen_  = 0;

        static uint64_t rotl64(uint64_t v, int n) { return (v << n) | (v >> (64 - n)); }

        void initState() {
            v1_ = seed_ + Prime1 + Prime2;
            v2_ = seed_ + Prime2;
            v3_ = seed_;
            v4_ = seed_ - Prime1;
        }

        static uint64_t round(uint64_t acc, uint64_t input) {
            acc += input * Prime2;
            acc = rotl64(acc, 31);
            acc *= Prime1;
            return acc;
        }

        static uint64_t mergeAccumulator(uint64_t h64, uint64_t acc) {
            h64 ^= round(0, acc);
            return h64 * Prime1 + Prime4;
        }

    public:
        explicit XxHash64(uint64_t seed = 0) : NonCryptographicHashAlgorithm(8), seed_(seed) {
            initState();
        }

        void Reset() override { totalLength_ = 0; bufLen_ = 0; initState(); }

        void Append(const uint8_t* source, size_t length) override {
            totalLength_ += length;
            const uint8_t* p = source;
            const uint8_t* end = source + length;

            if (bufLen_ > 0) {
                int need = 32 - bufLen_;
                int copy = static_cast<int>(length < static_cast<size_t>(need) ? length : static_cast<size_t>(need));
                std::memcpy(buf_ + bufLen_, p, static_cast<size_t>(copy));
                bufLen_ += copy;
                p += copy;
                if (bufLen_ < 32) return;
                processBlock(buf_);
                bufLen_ = 0;
            }

            while (p + 32 <= end) { processBlock(p); p += 32; }

            bufLen_ = static_cast<int>(end - p);
            if (bufLen_ > 0) std::memcpy(buf_, p, static_cast<size_t>(bufLen_));
        }

        void GetCurrentHash(uint8_t* dest, size_t /*len*/) override {
            uint64_t h64;
            if (totalLength_ >= 32) {
                h64 = rotl64(v1_, 1) + rotl64(v2_, 7) + rotl64(v3_, 12) + rotl64(v4_, 18);
                h64 = mergeAccumulator(h64, v1_);
                h64 = mergeAccumulator(h64, v2_);
                h64 = mergeAccumulator(h64, v3_);
                h64 = mergeAccumulator(h64, v4_);
            } else {
                h64 = seed_ + Prime5;
            }
            h64 += totalLength_;

            const uint8_t* p = buf_;
            int remaining = bufLen_;
            while (remaining >= 8) {
                uint64_t lane; std::memcpy(&lane, p, 8);
                h64 ^= round(0, lane);
                h64 = rotl64(h64, 27) * Prime1 + Prime4;
                p += 8; remaining -= 8;
            }
            if (remaining >= 4) {
                uint32_t lane; std::memcpy(&lane, p, 4);
                h64 ^= lane * Prime1;
                h64 = rotl64(h64, 23) * Prime2 + Prime3;
                p += 4; remaining -= 4;
            }
            while (remaining > 0) {
                h64 ^= (*p) * Prime5;
                h64 = rotl64(h64, 11) * Prime1;
                ++p; --remaining;
            }

            h64 ^= h64 >> 33; h64 *= Prime2;
            h64 ^= h64 >> 29; h64 *= Prime3;
            h64 ^= h64 >> 32;

            std::memcpy(dest, &h64, 8);
        }

        [[nodiscard]] uint64_t GetCurrentHashAsUInt64() {
            uint8_t buf[8];
            GetCurrentHash(buf, 8);
            uint64_t r; std::memcpy(&r, buf, 8); return r;
        }

        static uint64_t HashToUInt64(const std::vector<uint8_t>& source, uint64_t seed = 0) {
            XxHash64 h(seed);
            h.Append(source);
            return h.GetCurrentHashAsUInt64();
        }

    private:
        void processBlock(const uint8_t* block) {
            uint64_t lane;
            std::memcpy(&lane, block,      8); v1_ = round(v1_, lane);
            std::memcpy(&lane, block + 8,  8); v2_ = round(v2_, lane);
            std::memcpy(&lane, block + 16, 8); v3_ = round(v3_, lane);
            std::memcpy(&lane, block + 24, 8); v4_ = round(v4_, lane);
        }
    };

} // namespace System::IO::Hashing
