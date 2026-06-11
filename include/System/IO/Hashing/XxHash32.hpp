// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <vector>
#include "System/IO/Hashing/NonCryptographicHashAlgorithm.hpp"

namespace System::IO::Hashing {

    /**
     * @brief xxHash32 — fast 32-bit non-cryptographic hash (xxHash by Yann Collet).
     *
     * Partial C++ counterpart of .NET System.IO.Hashing.XxHash32.
     *
     * @note Status: Implemented
     */
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

        void initState();
        static uint32_t rotl32(uint32_t v, int n);
        static uint32_t round(uint32_t acc, uint32_t input);
        void processBlock(const uint8_t* block);

    public:
        /// @brief Constructs the hasher with an optional seed value.
        explicit XxHash32(uint32_t seed = 0);

        void Reset() override;

        using NonCryptographicHashAlgorithm::Append;
        void Append(const uint8_t* source, size_t length) override;
        void GetCurrentHash(uint8_t* dest, size_t len) override;

        /// @brief Returns the current hash as a native 32-bit integer.
        [[nodiscard]] uint32_t GetCurrentHashAsUInt32();

        /// @brief One-shot hash of the given byte vector.
        static uint32_t HashToUInt32(const std::vector<uint8_t>& source, uint32_t seed = 0);
    };

} // namespace System::IO::Hashing
