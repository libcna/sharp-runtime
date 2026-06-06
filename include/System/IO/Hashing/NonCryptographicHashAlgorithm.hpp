// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace System::IO::Hashing {

    class NonCryptographicHashAlgorithm {
        int hashLengthInBytes_;

    protected:
        explicit NonCryptographicHashAlgorithm(int hashLengthInBytes) {
            if (hashLengthInBytes < 1)
                throw std::out_of_range("hashLengthInBytes must be >= 1.");
            hashLengthInBytes_ = hashLengthInBytes;
        }

    public:
        virtual ~NonCryptographicHashAlgorithm() = default;

        [[nodiscard]] int getHashLengthInBytesProperty() const { return hashLengthInBytes_; }

        virtual void Append(const uint8_t* source, size_t length) = 0;
        virtual void Reset() = 0;
        virtual void GetCurrentHash(uint8_t* destination, size_t length) = 0;

        void Append(const std::vector<uint8_t>& source) {
            Append(source.data(), source.size());
        }

        [[nodiscard]] std::vector<uint8_t> GetCurrentHash() {
            std::vector<uint8_t> buf(static_cast<size_t>(hashLengthInBytes_));
            GetCurrentHash(buf.data(), buf.size());
            return buf;
        }

        [[nodiscard]] std::vector<uint8_t> GetHashAndReset() {
            auto result = GetCurrentHash();
            Reset();
            return result;
        }
    };

} // namespace System::IO::Hashing
