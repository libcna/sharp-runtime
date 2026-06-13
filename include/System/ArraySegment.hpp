// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;

    /// Delimits a section of a one-dimensional array.
    template<typename T>
    class ArraySegment {
        std::vector<T>* array_ = nullptr;
        intcs offset_ = 0;
        intcs count_  = 0;

    public:
        /// Initializes a new empty ArraySegment.
        ArraySegment() = default;

        /// Initializes a new ArraySegment that delimits all elements of the specified array.
        explicit ArraySegment(std::vector<T>& array)
            : array_(&array), offset_(0), count_(static_cast<intcs>(array.size())) {}

        /// Initializes a new ArraySegment that delimits a range of elements in the specified array.
        ArraySegment(std::vector<T>& array, intcs offset, intcs count)
            : array_(&array), offset_(offset), count_(count) {
            if (offset < 0 || count < 0 || offset + count > static_cast<intcs>(array.size()))
                throw std::out_of_range("ArraySegment: offset/count out of range");
        }

        /// Returns a pointer to the underlying array.
        [[nodiscard]] std::vector<T>* getArrayProperty() const { return array_; }
        /// Returns the position of the first element in the segment relative to the original array.
        [[nodiscard]] intcs getOffsetProperty() const { return offset_; }
        /// Returns the number of elements in the segment.
        [[nodiscard]] intcs getCountProperty() const { return count_; }

        /// Returns a reference to the element at the specified index within the segment.
        [[nodiscard]] T& operator[](intcs index) {
            if (index < 0 || index >= count_) throw std::out_of_range("ArraySegment: index out of range");
            return (*array_)[offset_ + index];
        }
        /// Returns a const reference to the element at the specified index within the segment.
        [[nodiscard]] const T& operator[](intcs index) const {
            if (index < 0 || index >= count_) throw std::out_of_range("ArraySegment: index out of range");
            return (*array_)[offset_ + index];
        }

        // Range-for support
        /// Returns a pointer to the first element of the segment.
        T* begin() { return array_ ? array_->data() + offset_ : nullptr; }
        /// Returns a pointer past the last element of the segment.
        T* end()   { return array_ ? array_->data() + offset_ + count_ : nullptr; }
        /// Returns a const pointer to the first element of the segment.
        const T* begin() const { return array_ ? array_->data() + offset_ : nullptr; }
        /// Returns a const pointer past the last element of the segment.
        const T* end()   const { return array_ ? array_->data() + offset_ + count_ : nullptr; }

        /// Returns a new ArraySegment that represents a sub-range of this segment.
        [[nodiscard]] ArraySegment<T> Slice(intcs index, intcs count) const {
            if (index < 0 || count < 0 || index + count > count_)
                throw std::out_of_range("ArraySegment::Slice: out of range");
            return ArraySegment<T>(*array_, offset_ + index, count);
        }
    };

} // namespace System
