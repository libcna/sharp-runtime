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

    /// <summary>
    /// Provides static methods for creating, manipulating, searching,
    /// and sorting arrays.
    ///
    /// Partial C++ counterpart of .NET System.Array.
    /// </summary>
    class Array {
    public:
        Array() = delete;

        /// Sorts all elements of @p array using the default less-than comparator.
        /// @param array Vector to sort in place.
        template<typename T>
        static void Sort(std::vector<T>& array) {
            std::sort(array.begin(), array.end());
        }

        /// Sorts all elements of @p array using the specified comparison function.
        /// @param array Vector to sort in place.
        /// @param comparison Function returning negative/zero/positive for a < b / a == b / a > b.
        template<typename T>
        static void Sort(std::vector<T>& array, std::function<int(const T&, const T&)> comparison) {
            std::sort(array.begin(), array.end(), [&](const T& a, const T& b) {
                return comparison(a, b) < 0;
            });
        }

        /// Copies @p length elements from @p src starting at @p srcIndex into @p dst starting at @p dstIndex.
        /// @param src Source vector.
        /// @param srcIndex Start index in the source.
        /// @param dst Destination vector (must already be sized).
        /// @param dstIndex Start index in the destination.
        /// @param length Number of elements to copy.
        template<typename T>
        static void Copy(const std::vector<T>& src, intcs srcIndex,
                         std::vector<T>& dst, intcs dstIndex, intcs length) {
            for (intcs i = 0; i < length; ++i)
                dst[dstIndex + i] = src[srcIndex + i];
        }

        /// Copies @p length elements from raw C-array @p src into @p dst using memcpy.
        /// @param src Source pointer.
        /// @param srcIndex Offset into @p src.
        /// @param dst Destination pointer.
        /// @param dstIndex Offset into @p dst.
        /// @param length Number of elements to copy.
        template<typename T>
        static void Copy(const T* src, intcs srcIndex, T* dst, intcs dstIndex, intcs length) {
            std::memcpy(dst + dstIndex, src + srcIndex, static_cast<size_t>(length) * sizeof(T));
        }

        /// Resizes @p array to @p newSize, preserving existing elements and default-initializing new ones.
        /// @param array Vector to resize.
        /// @param newSize Target element count.
        template<typename T>
        static void Resize(std::vector<T>& array, intcs newSize) {
            array.resize(static_cast<size_t>(newSize));
        }

        /// Returns the zero-based index of the first element equal to @p value, or -1 if not found.
        /// @param array Vector to search.
        /// @param value Value to find.
        template<typename T>
        static intcs IndexOf(const std::vector<T>& array, const T& value) {
            for (intcs i = 0; i < static_cast<intcs>(array.size()); ++i)
                if (array[i] == value) return i;
            return -1;
        }

        /// Returns the zero-based index of the first element equal to @p value starting at @p startIndex, or -1.
        template<typename T>
        static intcs IndexOf(const std::vector<T>& array, const T& value, intcs startIndex) {
            for (intcs i = startIndex; i < static_cast<intcs>(array.size()); ++i)
                if (array[static_cast<size_t>(i)] == value) return i;
            return -1;
        }

        /// Reverses the order of all elements in @p array in place.
        /// @param array Vector to reverse.
        template<typename T>
        static void Reverse(std::vector<T>& array) {
            std::reverse(array.begin(), array.end());
        }

        /// Resets @p length elements in @p array to the default value of T, starting at @p index.
        /// @param array Vector whose elements will be cleared.
        /// @param index Zero-based start index.
        /// @param length Number of elements to reset.
        template<typename T>
        static void Clear(std::vector<T>& array, intcs index, intcs length) {
            for (intcs i = index; i < index + length; ++i)
                array[i] = T{};
        }

        /// Searches a sorted @p array for @p value using binary search.
        /// @return Zero-based index if found; bitwise complement of insertion point otherwise.
        template<typename T>
        static intcs BinarySearch(const std::vector<T>& array, const T& value) {
            intcs lo = 0, hi = static_cast<intcs>(array.size()) - 1;
            while (lo <= hi) {
                intcs mid = lo + (hi - lo) / 2;
                if (array[static_cast<size_t>(mid)] == value) return mid;
                if (array[static_cast<size_t>(mid)] < value)  lo = mid + 1;
                else                                           hi = mid - 1;
            }
            return ~lo; // bitwise complement of insertion point
        }

        /// Sets every element in @p array to @p value.
        /// @param array Vector to fill.
        /// @param value Value to assign to every element.
        template<typename T>
        static void Fill(std::vector<T>& array, const T& value) {
            std::fill(array.begin(), array.end(), value);
        }

        /// Sets @p length elements in @p array starting at @p startIndex to @p value.
        template<typename T>
        static void Fill(std::vector<T>& array, const T& value, intcs startIndex, intcs count) {
            std::fill(array.begin() + startIndex, array.begin() + startIndex + count, value);
        }

        /// Returns an empty vector of type T (equivalent of .NET Array.Empty<T>()).
        template<typename T>
        [[nodiscard]] static std::vector<T> Empty() { return {}; }

        /// @brief Converts every element of @p array using @p converter; returns the results as a new vector.
        template<typename T, typename TOutput>
        [[nodiscard]] static std::vector<TOutput> ConvertAll(
                const std::vector<T>& array,
                std::function<TOutput(const T&)> converter) {
            std::vector<TOutput> result;
            result.reserve(array.size());
            for (const auto& item : array) result.push_back(converter(item));
            return result;
        }

        /// @brief Returns true if any element of @p array satisfies @p predicate.
        template<typename T>
        [[nodiscard]] static bool Exists(const std::vector<T>& array, std::function<bool(const T&)> predicate) {
            for (const auto& item : array)
                if (predicate(item)) return true;
            return false;
        }

        /// @brief Returns the first element satisfying @p predicate, or default T{} if none found.
        template<typename T>
        [[nodiscard]] static T Find(const std::vector<T>& array, std::function<bool(const T&)> predicate) {
            for (const auto& item : array)
                if (predicate(item)) return item;
            return T{};
        }

        /// @brief Returns the last element satisfying @p predicate, or default T{} if none found.
        template<typename T>
        [[nodiscard]] static T FindLast(const std::vector<T>& array, std::function<bool(const T&)> predicate) {
            for (intcs i = static_cast<intcs>(array.size()) - 1; i >= 0; --i)
                if (predicate(array[static_cast<size_t>(i)])) return array[static_cast<size_t>(i)];
            return T{};
        }

        /// @brief Returns a new vector of all elements satisfying @p predicate.
        template<typename T>
        [[nodiscard]] static std::vector<T> FindAll(
                const std::vector<T>& array,
                std::function<bool(const T&)> predicate) {
            std::vector<T> result;
            for (const auto& item : array)
                if (predicate(item)) result.push_back(item);
            return result;
        }

        /// @brief Returns the index of the first element satisfying @p predicate, or -1 if none.
        template<typename T>
        [[nodiscard]] static intcs FindIndex(
                const std::vector<T>& array,
                std::function<bool(const T&)> predicate) {
            for (intcs i = 0; i < static_cast<intcs>(array.size()); ++i)
                if (predicate(array[static_cast<size_t>(i)])) return i;
            return -1;
        }

        /// @brief Returns the index of the last element satisfying @p predicate, or -1 if none.
        template<typename T>
        [[nodiscard]] static intcs FindLastIndex(
                const std::vector<T>& array,
                std::function<bool(const T&)> predicate) {
            for (intcs i = static_cast<intcs>(array.size()) - 1; i >= 0; --i)
                if (predicate(array[static_cast<size_t>(i)])) return i;
            return -1;
        }

        /// @brief Applies @p action to every element of @p array.
        template<typename T>
        static void ForEach(const std::vector<T>& array, std::function<void(const T&)> action) {
            for (const auto& item : array) action(item);
        }

        /// @brief Returns true if all elements of @p array satisfy @p predicate (true for empty).
        template<typename T>
        [[nodiscard]] static bool TrueForAll(const std::vector<T>& array, std::function<bool(const T&)> predicate) {
            for (const auto& item : array)
                if (!predicate(item)) return false;
            return true;
        }

        /// @brief Returns the zero-based index of the last occurrence of @p value in @p array, or -1.
        template<typename T>
        [[nodiscard]] static intcs LastIndexOf(const std::vector<T>& array, const T& value) {
            for (intcs i = static_cast<intcs>(array.size()) - 1; i >= 0; --i)
                if (array[static_cast<size_t>(i)] == value) return i;
            return -1;
        }
    };

} // namespace System
