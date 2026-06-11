// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <vector>
#include "System/IO/Hashing/NonCryptographicHashAlgorithm.hpp"

namespace System::IO::Hashing {

    /**
     * @brief xxHash64 — fast 64-bit non-cryptographic hash (xxHash by Yann Collet).
     *
     * Partial C++ counterpart of .NET System.IO.Hashing.XxHash64.
     *
     * @note Status: Implemented
     */
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

        void initState();
        static uint64_t rotl64(uint64_t v, int n);
        static uint64_t round(uint64_t acc, uint64_t input);
        static uint64_t mergeAccumulator(uint64_t h64, uint64_t acc);
        void processBlock(const uint8_t* block);

    public:
        /// @brief Constructs the hasher with an optional seed value.
        explicit XxHash64(uint64_t seed = 0);

        void Reset() override;

        using NonCryptographicHashAlgorithm::Append;
        void Append(const uint8_t* source, size_t length) override;
        void GetCurrentHash(uint8_t* dest, size_t len) override;

        /// @brief Returns the current hash as a native 64-bit integer.
        [[nodiscard]] uint64_t GetCurrentHashAsUInt64();

        /// @brief One-shot hash of the given byte vector.
        static uint64_t HashToUInt64(const std::vector<uint8_t>& source, uint64_t seed = 0);
    };

} // namespace System::IO::Hashing
