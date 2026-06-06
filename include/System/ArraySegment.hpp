// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;

    template<typename T>
    class ArraySegment {
        std::vector<T>* array_ = nullptr;
        intcs offset_ = 0;
        intcs count_  = 0;

    public:
        ArraySegment() = default;

        explicit ArraySegment(std::vector<T>& array)
            : array_(&array), offset_(0), count_(static_cast<intcs>(array.size())) {}

        ArraySegment(std::vector<T>& array, intcs offset, intcs count)
            : array_(&array), offset_(offset), count_(count) {
            if (offset < 0 || count < 0 || offset + count > static_cast<intcs>(array.size()))
                throw std::out_of_range("ArraySegment: offset/count out of range");
        }

        [[nodiscard]] std::vector<T>* getArrayProperty() const { return array_; }
        [[nodiscard]] intcs getOffsetProperty() const { return offset_; }
        [[nodiscard]] intcs getCountProperty() const { return count_; }

        [[nodiscard]] T& operator[](intcs index) {
            if (index < 0 || index >= count_) throw std::out_of_range("ArraySegment: index out of range");
            return (*array_)[offset_ + index];
        }
        [[nodiscard]] const T& operator[](intcs index) const {
            if (index < 0 || index >= count_) throw std::out_of_range("ArraySegment: index out of range");
            return (*array_)[offset_ + index];
        }

        // Range-for support
        T* begin() { return array_ ? array_->data() + offset_ : nullptr; }
        T* end()   { return array_ ? array_->data() + offset_ + count_ : nullptr; }
        const T* begin() const { return array_ ? array_->data() + offset_ : nullptr; }
        const T* end()   const { return array_ ? array_->data() + offset_ + count_ : nullptr; }

        [[nodiscard]] ArraySegment<T> Slice(intcs index, intcs count) const {
            if (index < 0 || count < 0 || index + count > count_)
                throw std::out_of_range("ArraySegment::Slice: out of range");
            return ArraySegment<T>(*array_, offset_ + index, count);
        }
    };

} // namespace System
