// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <vector>
#include <stdexcept>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;

    /**
     * @brief Represents a contiguous region of read-only memory.
     *
     * Partial C++ counterpart of .NET System.ReadOnlyMemory<T>.
     * Wraps a const pointer and element count; does not own the data.
     *
     * @tparam T Element type.
     */
    template<typename T>
    class ReadOnlyMemory {
        const T* ptr_ = nullptr;
        intcs length_ = 0;

    public:
        /** @brief Constructs an empty ReadOnlyMemory. */
        constexpr ReadOnlyMemory() noexcept = default;

        /**
         * @brief Constructs from a raw const pointer and element count.
         * @param ptr   Pointer to the first element.
         * @param length Number of elements.
         */
        constexpr ReadOnlyMemory(const T* ptr, intcs length) noexcept
            : ptr_(ptr), length_(length) {}

        /**
         * @brief Constructs from a std::vector (borrows the data; does not copy).
         * @param v Source vector. Must outlive this ReadOnlyMemory.
         */
        explicit ReadOnlyMemory(const std::vector<T>& v) noexcept
            : ptr_(v.data()), length_(static_cast<intcs>(v.size())) {}

        /** @brief Returns the number of elements in this memory region. */
        [[nodiscard]] intcs getLengthProperty() const noexcept { return length_; }

        /** @brief Returns true if the memory region is empty (length == 0). */
        [[nodiscard]] bool getIsEmptyProperty() const noexcept { return length_ == 0; }

        /** @brief Returns a raw const pointer to the start of the memory region. */
        [[nodiscard]] const T* getPointer() const noexcept { return ptr_; }

        /**
         * @brief Element access by index.
         * @throws std::out_of_range if index is out of bounds.
         */
        [[nodiscard]] const T& operator[](intcs index) const {
            if (index < 0 || index >= length_)
                throw std::out_of_range("ReadOnlyMemory index out of range");
            return ptr_[index];
        }

        /**
         * @brief Returns a slice of this memory starting at @p start with @p length elements.
         * @throws std::out_of_range if the slice extends outside the current range.
         */
        [[nodiscard]] ReadOnlyMemory<T> Slice(intcs start, intcs length) const {
            if (start < 0 || length < 0 || start + length > length_)
                throw std::out_of_range("ReadOnlyMemory slice out of range");
            return ReadOnlyMemory<T>(ptr_ + start, length);
        }

        /**
         * @brief Returns a slice from @p start to the end of the memory region.
         * @throws std::out_of_range if start is out of bounds.
         */
        [[nodiscard]] ReadOnlyMemory<T> Slice(intcs start) const {
            return Slice(start, length_ - start);
        }

        /** @brief Returns an empty ReadOnlyMemory. */
        [[nodiscard]] static ReadOnlyMemory<T> Empty() noexcept { return {}; }

        /** @brief Copies the content into a new std::vector. */
        [[nodiscard]] std::vector<T> ToArray() const {
            return std::vector<T>(ptr_, ptr_ + length_);
        }
    };

} // namespace System
