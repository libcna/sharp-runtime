// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include <stdexcept>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;

    /// A type-safe, non-owning view over a contiguous region of mutable memory.
    ///
    /// C++ counterpart of .NET System.Span&lt;T&gt;. The caller is responsible for
    /// ensuring the underlying storage outlives the Span.
    template<typename T>
    class Span {
        T*     ptr_    = nullptr;
        intcs  length_ = 0;
    public:
        /// Constructs an empty Span (null pointer, zero length).
        Span() = default;
        /// Constructs a Span over @p length elements starting at @p ptr.
        Span(T* ptr, intcs length) : ptr_(ptr), length_(length) {}
        /// Constructs a Span covering the entire contents of vector @p v.
        explicit Span(std::vector<T>& v) : ptr_(v.data()), length_(static_cast<intcs>(v.size())) {}

        /// @return The number of elements in the span.
        [[nodiscard]] intcs getLengthProperty() const { return length_; }
        /// @return True if the span contains zero elements.
        [[nodiscard]] bool  getIsEmptyProperty() const { return length_ == 0; }

        /// @return A reference to element at @p i. Throws std::out_of_range if out of bounds.
        T& operator[](intcs i) {
            if (i < 0 || i >= length_) throw std::out_of_range("Span index out of range");
            return ptr_[i];
        }
        /// @return A const reference to element at @p i. Throws std::out_of_range if out of bounds.
        const T& operator[](intcs i) const {
            if (i < 0 || i >= length_) throw std::out_of_range("Span index out of range");
            return ptr_[i];
        }

        T*       begin()       { return ptr_; }           ///< Iterator to the first element.
        T*       end()         { return ptr_ + length_; } ///< Iterator past the last element.
        const T* begin() const { return ptr_; }           ///< Const iterator to the first element.
        const T* end()   const { return ptr_ + length_; } ///< Const iterator past the last element.

        /// @return A sub-span from @p start to the end of this span.
        [[nodiscard]] Span<T> Slice(intcs start) const { return Span<T>(ptr_ + start, length_ - start); }
        /// @return A sub-span of @p length elements starting at @p start.
        [[nodiscard]] Span<T> Slice(intcs start, intcs length) const { return Span<T>(ptr_ + start, length); }

        /// @return The raw pointer to the first element.
        [[nodiscard]] T* getPointer() { return ptr_; }
    };

    /// A type-safe, non-owning view over a contiguous region of read-only memory.
    ///
    /// C++ counterpart of .NET System.ReadOnlySpan&lt;T&gt;. The caller is responsible
    /// for ensuring the underlying storage outlives the ReadOnlySpan.
    template<typename T>
    class ReadOnlySpan {
        const T* ptr_    = nullptr;
        intcs    length_ = 0;
    public:
        /// Constructs an empty ReadOnlySpan.
        ReadOnlySpan() = default;
        /// Constructs a ReadOnlySpan over @p length elements starting at @p ptr.
        ReadOnlySpan(const T* ptr, intcs length) : ptr_(ptr), length_(length) {}
        /// Constructs a ReadOnlySpan covering the entire contents of vector @p v.
        explicit ReadOnlySpan(const std::vector<T>& v) : ptr_(v.data()), length_(static_cast<intcs>(v.size())) {}

        /// @return The number of elements in the span.
        [[nodiscard]] intcs getLengthProperty() const { return length_; }
        /// @return True if the span contains zero elements.
        [[nodiscard]] bool  getIsEmptyProperty() const { return length_ == 0; }

        /// @return A const reference to element at @p i. Throws std::out_of_range if out of bounds.
        const T& operator[](intcs i) const {
            if (i < 0 || i >= length_) throw std::out_of_range("ReadOnlySpan index out of range");
            return ptr_[i];
        }

        const T* begin() const { return ptr_; }           ///< Const iterator to the first element.
        const T* end()   const { return ptr_ + length_; } ///< Const iterator past the last element.

        /// @return A read-only sub-span from @p start to the end of this span.
        [[nodiscard]] ReadOnlySpan<T> Slice(intcs start) const { return ReadOnlySpan<T>(ptr_ + start, length_ - start); }
        /// @return A read-only sub-span of @p length elements starting at @p start.
        [[nodiscard]] ReadOnlySpan<T> Slice(intcs start, intcs length) const { return ReadOnlySpan<T>(ptr_ + start, length); }
    };

} // namespace System
