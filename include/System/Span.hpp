// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <typeinfo>
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
        /// @return A const raw pointer to the first element.
        [[nodiscard]] const T* getPointer() const noexcept { return ptr_; }
    };

    /**
     * @brief Provides a type-safe, non-owning read-only view over a contiguous region of memory.
     *
     * C++ counterpart of .NET System.ReadOnlySpan&lt;T&gt;.
     * The caller is responsible for ensuring the underlying storage outlives the span.
     * Unlike Span&lt;T&gt;, the elements cannot be mutated through this view.
     */
    template<typename T>
    class ReadOnlySpan {
        const T* ptr_    = nullptr;
        intcs    length_ = 0;
    public:
        /**
         * @brief Constructs an empty ReadOnlySpan (null pointer, zero length).
         *
         * C++ counterpart of .NET ReadOnlySpan&lt;T&gt; default constructor.
         */
        ReadOnlySpan() = default;

        /**
         * @brief Constructs a ReadOnlySpan over @p length elements starting at @p ptr.
         * @param ptr    Pointer to the first element.
         * @param length Number of elements.
         */
        ReadOnlySpan(const T* ptr, intcs length) : ptr_(ptr), length_(length) {}

        /**
         * @brief Constructs a ReadOnlySpan covering the entire contents of a vector.
         * @param v The source vector; must outlive this span.
         */
        explicit ReadOnlySpan(const std::vector<T>& v)
            : ptr_(v.data()), length_(static_cast<intcs>(v.size())) {}

        /**
         * @brief Constructs a ReadOnlySpan from a mutable Span.
         *
         * C++ counterpart of .NET implicit Span&lt;T&gt; → ReadOnlySpan&lt;T&gt; conversion.
         * @param span The source Span; must outlive this span.
         */
        ReadOnlySpan(const Span<T>& span)  // NOLINT(google-explicit-constructor)
            : ptr_(span.getPointer()), length_(span.getLengthProperty()) {}

        // -----------------------------------------------------------------------
        // Properties
        // -----------------------------------------------------------------------

        /**
         * @brief Gets the number of elements in the read-only span.
         *
         * C++ counterpart of .NET ReadOnlySpan&lt;T&gt;.Length.
         */
        [[nodiscard]] intcs getLengthProperty() const noexcept { return length_; }

        /**
         * @brief Gets a value indicating whether this span is empty.
         *
         * C++ counterpart of .NET ReadOnlySpan&lt;T&gt;.IsEmpty.
         */
        [[nodiscard]] bool getIsEmptyProperty() const noexcept { return length_ == 0; }

        /**
         * @brief Returns a read-only span with zero elements.
         *
         * C++ counterpart of .NET ReadOnlySpan&lt;T&gt;.Empty.
         */
        [[nodiscard]] static ReadOnlySpan<T> Empty() noexcept { return {}; }

        // -----------------------------------------------------------------------
        // Element access
        // -----------------------------------------------------------------------

        /**
         * @brief Returns a const reference to the element at the specified index.
         * @param i The zero-based index of the element.
         * @throws std::out_of_range if @p i is out of bounds.
         */
        const T& operator[](intcs i) const {
            if (i < 0 || i >= length_) throw std::out_of_range("ReadOnlySpan index out of range");
            return ptr_[i];
        }

        /** @brief Returns a const pointer to the first element. */
        [[nodiscard]] const T* getPointer() const noexcept { return ptr_; }

        /** @brief Const iterator to the first element. */
        const T* begin() const noexcept { return ptr_; }
        /** @brief Const iterator past the last element. */
        const T* end()   const noexcept { return ptr_ + length_; }

        // -----------------------------------------------------------------------
        // Slicing
        // -----------------------------------------------------------------------

        /**
         * @brief Forms a slice starting at @p start extending to the end of this span.
         * @param start The zero-based index at which to begin the slice.
         * @throws std::out_of_range if @p start is out of range.
         */
        [[nodiscard]] ReadOnlySpan<T> Slice(intcs start) const {
            if (start < 0 || start > length_) throw std::out_of_range("ReadOnlySpan::Slice start out of range");
            return ReadOnlySpan<T>(ptr_ + start, length_ - start);
        }

        /**
         * @brief Forms a slice of @p length elements starting at @p start.
         * @param start  The zero-based index at which to begin the slice.
         * @param length The number of elements in the slice.
         * @throws std::out_of_range if the range is invalid.
         */
        [[nodiscard]] ReadOnlySpan<T> Slice(intcs start, intcs length) const {
            if (start < 0 || length < 0 || start + length > length_)
                throw std::out_of_range("ReadOnlySpan::Slice range out of range");
            return ReadOnlySpan<T>(ptr_ + start, length);
        }

        // -----------------------------------------------------------------------
        // Copy and conversion
        // -----------------------------------------------------------------------

        /**
         * @brief Copies the contents of this read-only span into a destination Span.
         *
         * C++ counterpart of .NET ReadOnlySpan&lt;T&gt;.CopyTo(Span&lt;T&gt;).
         * @param destination The span to copy items into.
         * @throws std::invalid_argument if the destination is shorter than this span.
         */
        void CopyTo(Span<T> destination) const {
            if (length_ > destination.getLengthProperty())
                throw std::invalid_argument("Destination is too short.");
            std::copy(ptr_, ptr_ + length_, destination.getPointer());
        }

        /**
         * @brief Attempts to copy this read-only span into a destination Span.
         *
         * C++ counterpart of .NET ReadOnlySpan&lt;T&gt;.TryCopyTo(Span&lt;T&gt;).
         * @param destination The span to copy items into.
         * @return true if the copy succeeded; false if the destination is too short.
         */
        [[nodiscard]] bool TryCopyTo(Span<T> destination) const noexcept {
            if (length_ > destination.getLengthProperty()) return false;
            std::copy(ptr_, ptr_ + length_, destination.getPointer());
            return true;
        }

        /**
         * @brief Copies the contents of this read-only span into a new vector.
         *
         * C++ counterpart of .NET ReadOnlySpan&lt;T&gt;.ToArray().
         * @return A new std::vector&lt;T&gt; containing the elements of this span.
         */
        [[nodiscard]] std::vector<T> ToArray() const {
            return std::vector<T>(ptr_, ptr_ + length_);
        }

        /**
         * @brief Returns a string representation of this span.
         *
         * C++ counterpart of .NET ReadOnlySpan&lt;T&gt;.ToString().
         * For char spans, returns the characters as a string.
         * For other types, returns a type descriptor.
         */
        [[nodiscard]] std::string ToString() const {
            if constexpr (std::is_same_v<T, char>) {
                return std::string(ptr_, static_cast<std::size_t>(length_));
            } else {
                return std::string("System.ReadOnlySpan<")
                     + typeid(T).name() + ">["
                     + std::to_string(length_) + "]";
            }
        }

        // -----------------------------------------------------------------------
        // Comparison operators
        // -----------------------------------------------------------------------

        /**
         * @brief Returns true if both spans point to the same memory and have the same length.
         *
         * C++ counterpart of .NET ReadOnlySpan&lt;T&gt;.operator ==.
         * Does NOT compare element contents.
         */
        [[nodiscard]] bool operator==(const ReadOnlySpan<T>& o) const noexcept {
            return ptr_ == o.ptr_ && length_ == o.length_;
        }

        /**
         * @brief Returns true if the spans differ in pointer or length.
         *
         * C++ counterpart of .NET ReadOnlySpan&lt;T&gt;.operator !=.
         */
        [[nodiscard]] bool operator!=(const ReadOnlySpan<T>& o) const noexcept {
            return !(*this == o);
        }
    };

} // namespace System
