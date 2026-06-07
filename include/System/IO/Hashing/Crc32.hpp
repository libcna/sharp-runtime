// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <vector>
#include "System/IO/Hashing/NonCryptographicHashAlgorithm.hpp"

namespace System::IO::Hashing {

    class Crc32 : public NonCryptographicHashAlgorithm {
        uint32_t crc_ = 0xFFFFFFFFu;

        static uint32_t table_[256];
        static bool tableInitialized_;

        static void initTable() {
            if (tableInitialized_) return;
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int j = 0; j < 8; ++j)
                    c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                table_[i] = c;
            }
            tableInitialized_ = true;
        }

    public:
        Crc32() : NonCryptographicHashAlgorithm(4) { initTable(); }

        using NonCryptographicHashAlgorithm::Append;
        void Append(const uint8_t* source, size_t length) override {
            for (size_t i = 0; i < length; ++i)
                crc_ = table_[(crc_ ^ source[i]) & 0xFF] ^ (crc_ >> 8);
        }

        void Reset() override { crc_ = 0xFFFFFFFFu; }

        void GetCurrentHash(uint8_t* dest, size_t /*len*/) override {
            uint32_t result = crc_ ^ 0xFFFFFFFFu;
            dest[0] = static_cast<uint8_t>(result & 0xFF);
            dest[1] = static_cast<uint8_t>((result >> 8) & 0xFF);
            dest[2] = static_cast<uint8_t>((result >> 16) & 0xFF);
            dest[3] = static_cast<uint8_t>((result >> 24) & 0xFF);
        }

        [[nodiscard]] uint32_t GetCurrentHashAsUInt32() const {
            return crc_ ^ 0xFFFFFFFFu;
        }

        static uint32_t HashToUInt32(const std::vector<uint8_t>& source) {
            Crc32 h;
            h.Append(source);
            return h.GetCurrentHashAsUInt32();
        }
    };

    inline uint32_t Crc32::table_[256] = {};
    inline bool Crc32::tableInitialized_ = false;

} // namespace System::IO::Hashing
