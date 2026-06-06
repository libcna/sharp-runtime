// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <algorithm>
#include <cstring>
#include <functional>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    using SharpRuntime::intcs;

    /**
     * @brief Provides static methods for creating, manipulating, searching,
     * and sorting arrays.
     *
     * Partial C++ counterpart of .NET System.Array.
     *
     * @note Status: Partial
     */
    class Array {
    public:
        Array() = delete;

        /**
         * @brief Sorts the elements in a vector using the default comparer.
         */
        template<typename T>
        static void Sort(std::vector<T>& array) {
            std::sort(array.begin(), array.end());
        }

        /**
         * @brief Sorts the elements in a vector using the specified comparison.
         */
        template<typename T>
        static void Sort(std::vector<T>& array, std::function<int(const T&, const T&)> comparison) {
            std::sort(array.begin(), array.end(), [&](const T& a, const T& b) {
                return comparison(a, b) < 0;
            });
        }

        /**
         * @brief Copies a range of elements from one vector to another.
         */
        template<typename T>
        static void Copy(const std::vector<T>& src, intcs srcIndex,
                         std::vector<T>& dst, intcs dstIndex, intcs length) {
            for (intcs i = 0; i < length; ++i)
                dst[dstIndex + i] = src[srcIndex + i];
        }

        /**
         * @brief Copies from raw C-array to vector.
         */
        template<typename T>
        static void Copy(const T* src, intcs srcIndex, T* dst, intcs dstIndex, intcs length) {
            std::memcpy(dst + dstIndex, src + srcIndex, static_cast<size_t>(length) * sizeof(T));
        }

        /**
         * @brief Changes the size of the vector to the specified new size,
         * preserving existing elements.
         */
        template<typename T>
        static void Resize(std::vector<T>& array, intcs newSize) {
            array.resize(static_cast<size_t>(newSize));
        }

        /**
         * @brief Searches for the specified element and returns the index of
         * its first occurrence in the vector.
         */
        template<typename T>
        static intcs IndexOf(const std::vector<T>& array, const T& value) {
            for (intcs i = 0; i < static_cast<intcs>(array.size()); ++i)
                if (array[i] == value) return i;
            return -1;
        }

        /**
         * @brief Reverses the sequence of the elements in the entire vector.
         */
        template<typename T>
        static void Reverse(std::vector<T>& array) {
            std::reverse(array.begin(), array.end());
        }

        /**
         * @brief Sets a range of elements in the vector to the default value
         * of each element type.
         */
        template<typename T>
        static void Clear(std::vector<T>& array, intcs index, intcs length) {
            for (intcs i = index; i < index + length; ++i)
                array[i] = T{};
        }
    };

} // namespace System
