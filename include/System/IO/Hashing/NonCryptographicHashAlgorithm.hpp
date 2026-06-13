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

        /// Returns the size of the hash produced by this algorithm, in bytes.
        [[nodiscard]] int getHashLengthInBytesProperty() const { return hashLengthInBytes_; }

        /// Appends the given byte data to the data already processed by the algorithm.
        virtual void Append(const uint8_t* source, size_t length) = 0;
        /// Resets the hash algorithm to its initial state.
        virtual void Reset() = 0;
        /// Writes the current hash value into destination.
        virtual void GetCurrentHash(uint8_t* destination, size_t length) = 0;

        /// Appends a vector of bytes to the data already processed.
        void Append(const std::vector<uint8_t>& source) {
            Append(source.data(), source.size());
        }

        /// Returns the current hash value as a byte vector.
        [[nodiscard]] std::vector<uint8_t> GetCurrentHash() {
            std::vector<uint8_t> buf(static_cast<size_t>(hashLengthInBytes_));
            GetCurrentHash(buf.data(), buf.size());
            return buf;
        }

        /// Returns the current hash value and resets the algorithm.
        [[nodiscard]] std::vector<uint8_t> GetHashAndReset() {
            auto result = GetCurrentHash();
            Reset();
            return result;
        }
    };

} // namespace System::IO::Hashing
