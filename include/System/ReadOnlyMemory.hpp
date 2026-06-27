// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Span.hpp"

namespace System {

    using SharpRuntime::intcs;

    /**
     * @brief Represents a contiguous region of read-only memory.
     *
     * C++ counterpart of .NET System.ReadOnlyMemory<T>. Wraps a const pointer and
     * element count; does not own the data. Equivalent to a non-owning view over an
     * array or contiguous buffer.
     *
     * @tparam T Element type.
     */
    template<typename T>
    class ReadOnlyMemory {
        const T* ptr_ = nullptr;
        intcs    length_ = 0;

    public:
        /** @brief Constructs an empty ReadOnlyMemory. Equivalent to ReadOnlyMemory<T>.Empty. */
        constexpr ReadOnlyMemory() noexcept = default;

        /**
         * @brief Constructs from a raw const pointer and element count.
         * @param ptr    Pointer to the first element.
         * @param length Number of elements.
         */
        constexpr ReadOnlyMemory(const T* ptr, intcs length) noexcept
            : ptr_(ptr), length_(length) {}

        /**
         * @brief Constructs from a std::vector, borrowing its data without copying.
         *
         * The vector must outlive this ReadOnlyMemory.
         * @param v Source vector.
         */
        explicit ReadOnlyMemory(const std::vector<T>& v) noexcept
            : ptr_(v.data()), length_(static_cast<intcs>(v.size())) {}

        /**
         * @brief Returns the number of elements in this memory region.
         *
         * C++ counterpart of .NET ReadOnlyMemory<T>.Length.
         */
        [[nodiscard]] intcs getLengthProperty() const noexcept { return length_; }

        /**
         * @brief Returns true if the memory region contains no elements.
         *
         * C++ counterpart of .NET ReadOnlyMemory<T>.IsEmpty.
         */
        [[nodiscard]] bool getIsEmptyProperty() const noexcept { return length_ == 0; }

        /**
         * @brief Returns a ReadOnlySpan<T> over the memory region.
         *
         * C++ counterpart of .NET ReadOnlyMemory<T>.Span.
         */
        [[nodiscard]] ReadOnlySpan<T> getSpanProperty() const noexcept {
            return ReadOnlySpan<T>(ptr_, length_);
        }

        /** @brief Returns a raw const pointer to the start of the memory region. */
        [[nodiscard]] const T* getPointer() const noexcept { return ptr_; }

        /**
         * @brief Returns an empty ReadOnlyMemory<T>.
         *
         * C++ counterpart of .NET ReadOnlyMemory<T>.Empty.
         */
        [[nodiscard]] static ReadOnlyMemory<T> getEmptyProperty() noexcept { return {}; }

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
         *
         * C++ counterpart of .NET ReadOnlyMemory<T>.Slice(int, int).
         * @throws std::out_of_range if the slice extends outside the current range.
         */
        [[nodiscard]] ReadOnlyMemory<T> Slice(intcs start, intcs length) const {
            if (start < 0 || length < 0 || start + length > length_)
                throw std::out_of_range("ReadOnlyMemory slice out of range");
            return ReadOnlyMemory<T>(ptr_ + start, length);
        }

        /**
         * @brief Returns a slice from @p start to the end of the memory region.
         *
         * C++ counterpart of .NET ReadOnlyMemory<T>.Slice(int).
         * @throws std::out_of_range if start is out of bounds.
         */
        [[nodiscard]] ReadOnlyMemory<T> Slice(intcs start) const {
            return Slice(start, length_ - start);
        }

        /**
         * @brief Copies the content into a new std::vector.
         *
         * C++ counterpart of .NET ReadOnlyMemory<T>.ToArray().
         */
        [[nodiscard]] std::vector<T> ToArray() const {
            return std::vector<T>(ptr_, ptr_ + length_);
        }

        /**
         * @brief Returns a string description of this memory region.
         *
         * C++ counterpart of .NET ReadOnlyMemory<T>.ToString().
         */
        [[nodiscard]] std::string ToString() const {
            return "System.ReadOnlyMemory<T>[" + std::to_string(length_) + "]";
        }

        /**
         * @brief Returns true if this ReadOnlyMemory<T> refers to the same region as @p other.
         *
         * C++ counterpart of .NET ReadOnlyMemory<T>.Equals(ReadOnlyMemory<T>).
         * Two instances are equal when they point to the same start address with the same length.
         */
        [[nodiscard]] bool Equals(const ReadOnlyMemory<T>& other) const noexcept {
            return ptr_ == other.ptr_ && length_ == other.length_;
        }

        /** @brief Returns true if the two instances refer to the same memory region. */
        friend bool operator==(const ReadOnlyMemory<T>& lhs, const ReadOnlyMemory<T>& rhs) noexcept {
            return lhs.Equals(rhs);
        }

        /** @brief Returns true if the two instances do not refer to the same memory region. */
        friend bool operator!=(const ReadOnlyMemory<T>& lhs, const ReadOnlyMemory<T>& rhs) noexcept {
            return !lhs.Equals(rhs);
        }
    };

} // namespace System
