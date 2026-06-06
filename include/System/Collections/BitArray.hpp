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
        explicit BitArray(intcs length, bool defaultValue = false)
            : bits_(static_cast<size_t>(length), defaultValue) {}

        explicit BitArray(const std::vector<bool>& values) : bits_(values) {}

        explicit BitArray(const std::vector<SharpRuntime::bytecs>& bytes) {
            bits_.reserve(bytes.size() * 8);
            for (auto b : bytes)
                for (int i = 0; i < 8; ++i)
                    bits_.push_back((b >> i) & 1);
        }

        [[nodiscard]] intcs getLengthProperty() const { return static_cast<intcs>(bits_.size()); }
        [[nodiscard]] intcs getCountProperty()  const { return getLengthProperty(); }

        [[nodiscard]] bool Get(intcs index) const { return bits_.at(static_cast<size_t>(index)); }
        void Set(intcs index, bool value)         { bits_.at(static_cast<size_t>(index)) = value; }
        void SetAll(bool value)                   { std::fill(bits_.begin(), bits_.end(), value); }

        bool operator[](intcs i) const { return bits_.at(static_cast<size_t>(i)); }

        BitArray& And(const BitArray& other) {
            for (size_t i = 0; i < bits_.size(); ++i) bits_[i] = bits_[i] && other.bits_[i];
            return *this;
        }
        BitArray& Or(const BitArray& other) {
            for (size_t i = 0; i < bits_.size(); ++i) bits_[i] = bits_[i] || other.bits_[i];
            return *this;
        }
        BitArray& Xor(const BitArray& other) {
            for (size_t i = 0; i < bits_.size(); ++i) bits_[i] = bits_[i] != other.bits_[i];
            return *this;
        }
        BitArray& Not() {
            for (size_t i = 0; i < bits_.size(); ++i) bits_[i] = !bits_[i];
            return *this;
        }

        auto begin() const { return bits_.begin(); }
        auto end()   const { return bits_.end(); }
    };

} // namespace System::Collections
