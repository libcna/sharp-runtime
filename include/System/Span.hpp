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

    /**
     * @brief Provides a type-safe view over a contiguous region of arbitrary memory.
     *
     * Partial C++ counterpart of .NET System.Span<T>.
     * Does NOT own memory — the caller must ensure the pointed-to data outlives the Span.
     *
     * @note Status: Partial
     */
    template<typename T>
    class Span {
        T*     ptr_    = nullptr;
        intcs  length_ = 0;
    public:
        Span() = default;
        Span(T* ptr, intcs length) : ptr_(ptr), length_(length) {}
        explicit Span(std::vector<T>& v) : ptr_(v.data()), length_(static_cast<intcs>(v.size())) {}

        [[nodiscard]] intcs getLengthProperty() const { return length_; }
        [[nodiscard]] bool  getIsEmptyProperty() const { return length_ == 0; }

        T& operator[](intcs i) {
            if (i < 0 || i >= length_) throw std::out_of_range("Span index out of range");
            return ptr_[i];
        }
        const T& operator[](intcs i) const {
            if (i < 0 || i >= length_) throw std::out_of_range("Span index out of range");
            return ptr_[i];
        }

        T*       begin()       { return ptr_; }
        T*       end()         { return ptr_ + length_; }
        const T* begin() const { return ptr_; }
        const T* end()   const { return ptr_ + length_; }

        [[nodiscard]] Span<T> Slice(intcs start) const { return Span<T>(ptr_ + start, length_ - start); }
        [[nodiscard]] Span<T> Slice(intcs start, intcs length) const { return Span<T>(ptr_ + start, length); }

        [[nodiscard]] T* getPointer() { return ptr_; }
    };

    template<typename T>
    class ReadOnlySpan {
        const T* ptr_    = nullptr;
        intcs    length_ = 0;
    public:
        ReadOnlySpan() = default;
        ReadOnlySpan(const T* ptr, intcs length) : ptr_(ptr), length_(length) {}
        explicit ReadOnlySpan(const std::vector<T>& v) : ptr_(v.data()), length_(static_cast<intcs>(v.size())) {}

        [[nodiscard]] intcs getLengthProperty() const { return length_; }
        [[nodiscard]] bool  getIsEmptyProperty() const { return length_ == 0; }

        const T& operator[](intcs i) const {
            if (i < 0 || i >= length_) throw std::out_of_range("ReadOnlySpan index out of range");
            return ptr_[i];
        }

        const T* begin() const { return ptr_; }
        const T* end()   const { return ptr_ + length_; }

        [[nodiscard]] ReadOnlySpan<T> Slice(intcs start) const { return ReadOnlySpan<T>(ptr_ + start, length_ - start); }
        [[nodiscard]] ReadOnlySpan<T> Slice(intcs start, intcs length) const { return ReadOnlySpan<T>(ptr_ + start, length); }
    };

} // namespace System
