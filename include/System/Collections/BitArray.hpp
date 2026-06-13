// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <vector>
#include <stdexcept>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections {

    using SharpRuntime::intcs;

    /**
     * @brief Manages a compact array of bit values, represented as Booleans.
     *
     * Wraps std::vector<bool>. Partial C++ counterpart of .NET System.Collections.BitArray.
     *
     * @note Status: Partial
     */
    class BitArray {
        std::vector<bool> bits_;
    public:
        /// Constructs a BitArray with the given length, optionally initializing all bits to a default value.
        explicit BitArray(intcs length, bool defaultValue = false)
            : bits_(static_cast<size_t>(length), defaultValue) {}

        /// Constructs a BitArray from an existing vector of bool values.
        explicit BitArray(const std::vector<bool>& values) : bits_(values) {}

        /// Constructs a BitArray from a vector of bytes, unpacking each byte into 8 bits.
        explicit BitArray(const std::vector<SharpRuntime::bytecs>& bytes) {
            bits_.reserve(bytes.size() * 8);
            for (auto b : bytes)
                for (int i = 0; i < 8; ++i)
                    bits_.push_back((b >> i) & 1);
        }

        /// Gets the number of bits in the BitArray.
        [[nodiscard]] intcs getLengthProperty() const { return static_cast<intcs>(bits_.size()); }
        /// Gets the number of elements (bits) in the BitArray.
        [[nodiscard]] intcs getCountProperty()  const { return getLengthProperty(); }

        /// Returns the value of the bit at the given index.
        [[nodiscard]] bool Get(intcs index) const { return bits_.at(static_cast<size_t>(index)); }
        /// Sets the bit at the given index to the specified value.
        void Set(intcs index, bool value)         { bits_.at(static_cast<size_t>(index)) = value; }
        /// Sets all bits in the BitArray to the specified value.
        void SetAll(bool value)                   { std::fill(bits_.begin(), bits_.end(), value); }

        /// Returns the value of the bit at index i.
        bool operator[](intcs i) const { return bits_.at(static_cast<size_t>(i)); }

        /// Performs a bitwise AND of this BitArray with another and returns *this.
        BitArray& And(const BitArray& other) {
            for (size_t i = 0; i < bits_.size(); ++i) bits_[i] = bits_[i] && other.bits_[i];
            return *this;
        }
        /// Performs a bitwise OR of this BitArray with another and returns *this.
        BitArray& Or(const BitArray& other) {
            for (size_t i = 0; i < bits_.size(); ++i) bits_[i] = bits_[i] || other.bits_[i];
            return *this;
        }
        /// Performs a bitwise XOR of this BitArray with another and returns *this.
        BitArray& Xor(const BitArray& other) {
            for (size_t i = 0; i < bits_.size(); ++i) bits_[i] = bits_[i] != other.bits_[i];
            return *this;
        }
        /// Inverts all bit values in this BitArray and returns *this.
        BitArray& Not() {
            for (size_t i = 0; i < bits_.size(); ++i) bits_[i] = !bits_[i];
            return *this;
        }

        /// Returns an iterator to the beginning of the bit sequence.
        auto begin() const { return bits_.begin(); }
        /// Returns an iterator past the end of the bit sequence.
        auto end()   const { return bits_.end(); }
    };

} // namespace System::Collections
