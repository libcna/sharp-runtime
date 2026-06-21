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
     * @brief Provides static methods for creating, manipulating, searching, and sorting arrays.
     *
     * Partial C++ counterpart of .NET System.Array.
     * All methods operate on std::vector<T> rather than raw C-style arrays.
     */
    class Array {
    public:
        Array() = delete;

        /** @brief Sorts all elements of @p array using the default less-than comparator. */
        template<typename T>
        static void Sort(std::vector<T>& array) {
            std::sort(array.begin(), array.end());
        }

        /**
         * @brief Sorts all elements of @p array using @p comparison.
         * @param comparison Function returning negative/zero/positive for a&lt;b / a==b / a&gt;b.
         */
        template<typename T>
        static void Sort(std::vector<T>& array, std::function<int(const T&, const T&)> comparison) {
            std::sort(array.begin(), array.end(), [&](const T& a, const T& b) {
                return comparison(a, b) < 0;
            });
        }

        /**
         * @brief Copies @p length elements from @p src[@p srcIndex] into @p dst[@p dstIndex].
         * @param dst Destination vector (must already be sized to fit the copied range).
         */
        template<typename T>
        static void Copy(const std::vector<T>& src, intcs srcIndex,
                         std::vector<T>& dst, intcs dstIndex, intcs length) {
            for (intcs i = 0; i < length; ++i)
                dst[dstIndex + i] = src[srcIndex + i];
        }

        /**
         * @brief Copies @p length elements from raw C-array @p src into @p dst using memcpy.
         */
        template<typename T>
        static void Copy(const T* src, intcs srcIndex, T* dst, intcs dstIndex, intcs length) {
            std::memcpy(dst + dstIndex, src + srcIndex, static_cast<size_t>(length) * sizeof(T));
        }

        /** @brief Resizes @p array to @p newSize, preserving existing elements and default-initializing new ones. */
        template<typename T>
        static void Resize(std::vector<T>& array, intcs newSize) {
            array.resize(static_cast<size_t>(newSize));
        }

        /** @brief Returns the zero-based index of the first element equal to @p value, or -1 if not found. */
        template<typename T>
        static intcs IndexOf(const std::vector<T>& array, const T& value) {
            for (intcs i = 0; i < static_cast<intcs>(array.size()); ++i)
                if (array[i] == value) return i;
            return -1;
        }

        /** @brief Returns the first index of @p value starting at @p startIndex, or -1. */
        template<typename T>
        static intcs IndexOf(const std::vector<T>& array, const T& value, intcs startIndex) {
            for (intcs i = startIndex; i < static_cast<intcs>(array.size()); ++i)
                if (array[static_cast<size_t>(i)] == value) return i;
            return -1;
        }

        /** @brief Reverses the order of all elements in @p array in place. */
        template<typename T>
        static void Reverse(std::vector<T>& array) {
            std::reverse(array.begin(), array.end());
        }

        /** @brief Resets @p length elements starting at @p index to the default value of T. */
        template<typename T>
        static void Clear(std::vector<T>& array, intcs index, intcs length) {
            for (intcs i = index; i < index + length; ++i)
                array[i] = T{};
        }

        /**
         * @brief Searches a sorted @p array for @p value using binary search.
         * @return Zero-based index if found; bitwise complement of the insertion point otherwise.
         */
        template<typename T>
        static intcs BinarySearch(const std::vector<T>& array, const T& value) {
            intcs lo = 0, hi = static_cast<intcs>(array.size()) - 1;
            while (lo <= hi) {
                intcs mid = lo + (hi - lo) / 2;
                if (array[static_cast<size_t>(mid)] == value) return mid;
                if (array[static_cast<size_t>(mid)] < value)  lo = mid + 1;
                else                                           hi = mid - 1;
            }
            return ~lo;
        }

        /** @brief Sets every element in @p array to @p value. */
        template<typename T>
        static void Fill(std::vector<T>& array, const T& value) {
            std::fill(array.begin(), array.end(), value);
        }

        /** @brief Sets @p count elements starting at @p startIndex to @p value. */
        template<typename T>
        static void Fill(std::vector<T>& array, const T& value, intcs startIndex, intcs count) {
            std::fill(array.begin() + startIndex, array.begin() + startIndex + count, value);
        }

        /** @brief Returns an empty vector of type T (equivalent of .NET Array.Empty&lt;T&gt;()). */
        template<typename T>
        [[nodiscard]] static std::vector<T> Empty() { return {}; }

        /** @brief Converts every element using @p converter and returns the results as a new vector. */
        template<typename T, typename TOutput>
        [[nodiscard]] static std::vector<TOutput> ConvertAll(
                const std::vector<T>& array,
                std::function<TOutput(const T&)> converter) {
            std::vector<TOutput> result;
            result.reserve(array.size());
            for (const auto& item : array) result.push_back(converter(item));
            return result;
        }

        /** @brief Returns true if any element of @p array satisfies @p predicate. */
        template<typename T>
        [[nodiscard]] static bool Exists(const std::vector<T>& array, std::function<bool(const T&)> predicate) {
            for (const auto& item : array)
                if (predicate(item)) return true;
            return false;
        }

        /** @brief Returns the first element satisfying @p predicate, or default T{} if none found. */
        template<typename T>
        [[nodiscard]] static T Find(const std::vector<T>& array, std::function<bool(const T&)> predicate) {
            for (const auto& item : array)
                if (predicate(item)) return item;
            return T{};
        }

        /** @brief Returns the last element satisfying @p predicate, or default T{} if none found. */
        template<typename T>
        [[nodiscard]] static T FindLast(const std::vector<T>& array, std::function<bool(const T&)> predicate) {
            for (intcs i = static_cast<intcs>(array.size()) - 1; i >= 0; --i)
                if (predicate(array[static_cast<size_t>(i)])) return array[static_cast<size_t>(i)];
            return T{};
        }

        /** @brief Returns a new vector of all elements satisfying @p predicate. */
        template<typename T>
        [[nodiscard]] static std::vector<T> FindAll(
                const std::vector<T>& array,
                std::function<bool(const T&)> predicate) {
            std::vector<T> result;
            for (const auto& item : array)
                if (predicate(item)) result.push_back(item);
            return result;
        }

        /** @brief Returns the index of the first element satisfying @p predicate, or -1 if none. */
        template<typename T>
        [[nodiscard]] static intcs FindIndex(
                const std::vector<T>& array,
                std::function<bool(const T&)> predicate) {
            for (intcs i = 0; i < static_cast<intcs>(array.size()); ++i)
                if (predicate(array[static_cast<size_t>(i)])) return i;
            return -1;
        }

        /** @brief Returns the index of the last element satisfying @p predicate, or -1 if none. */
        template<typename T>
        [[nodiscard]] static intcs FindLastIndex(
                const std::vector<T>& array,
                std::function<bool(const T&)> predicate) {
            for (intcs i = static_cast<intcs>(array.size()) - 1; i >= 0; --i)
                if (predicate(array[static_cast<size_t>(i)])) return i;
            return -1;
        }

        /** @brief Applies @p action to every element of @p array. */
        template<typename T>
        static void ForEach(const std::vector<T>& array, std::function<void(const T&)> action) {
            for (const auto& item : array) action(item);
        }

        /** @brief Returns true if all elements satisfy @p predicate (vacuously true for empty arrays). */
        template<typename T>
        [[nodiscard]] static bool TrueForAll(const std::vector<T>& array, std::function<bool(const T&)> predicate) {
            for (const auto& item : array)
                if (!predicate(item)) return false;
            return true;
        }

        /** @brief Returns the zero-based index of the last occurrence of @p value in @p array, or -1. */
        template<typename T>
        [[nodiscard]] static intcs LastIndexOf(const std::vector<T>& array, const T& value) {
            for (intcs i = static_cast<intcs>(array.size()) - 1; i >= 0; --i)
                if (array[static_cast<size_t>(i)] == value) return i;
            return -1;
        }
    };

} // namespace System
