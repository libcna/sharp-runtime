// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>
#include "System/Span.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    /**
     * @brief Extension-like static methods for Span<T> and ReadOnlySpan<T>.
     *
     * Partial C++ counterpart of .NET System.MemoryExtensions.
     * Covers the subset most relevant to game-dev / XNA porting:
     * AsSpan, Contains, ContainsAny, IndexOf, LastIndexOf, SequenceEqual,
     * SequenceCompareTo, Fill, Reverse, CopyTo, Sort, StartsWith, EndsWith,
     * Trim/TrimStart/TrimEnd, Count, BinarySearch, Overlaps.
     */
    struct MemoryExtensions {
        MemoryExtensions() = delete;

        // ---------------------------------------------------------------
        // AsSpan — create a Span/ReadOnlySpan over a vector
        // ---------------------------------------------------------------

        /**
         * @brief Returns a Span over the entire vector.
         *
         * C++ counterpart of .NET MemoryExtensions.AsSpan<T>(T[]).
         */
        template<typename T>
        [[nodiscard]] static Span<T> AsSpan(std::vector<T>& v) {
            return Span<T>(v);
        }

        /**
         * @brief Returns a Span over vector elements starting at @p start.
         *
         * C++ counterpart of .NET MemoryExtensions.AsSpan<T>(T[], int).
         */
        template<typename T>
        [[nodiscard]] static Span<T> AsSpan(std::vector<T>& v, SharpRuntime::intcs start) {
            return Span<T>(v.data() + start, static_cast<SharpRuntime::intcs>(v.size()) - start);
        }

        /**
         * @brief Returns a Span over @p length elements of vector starting at @p start.
         *
         * C++ counterpart of .NET MemoryExtensions.AsSpan<T>(T[], int, int).
         */
        template<typename T>
        [[nodiscard]] static Span<T> AsSpan(std::vector<T>& v, SharpRuntime::intcs start,
                                             SharpRuntime::intcs length) {
            return Span<T>(v.data() + start, length);
        }

        /**
         * @brief Returns a ReadOnlySpan over the entire vector.
         *
         * C++ counterpart of .NET MemoryExtensions.AsSpan<T>(ImmutableArray<T>).
         */
        template<typename T>
        [[nodiscard]] static ReadOnlySpan<T> AsReadOnlySpan(const std::vector<T>& v) {
            return ReadOnlySpan<T>(v);
        }

        // ---------------------------------------------------------------
        // AsSpan — create a ReadOnlySpan<char> over a std::string
        // ---------------------------------------------------------------

        /**
         * @brief Creates a new ReadOnlySpan<char> over the entire string.
         *
         * C++ counterpart of .NET MemoryExtensions.AsSpan(string).
         * @param text The source string.
         * @return A ReadOnlySpan<char> over @p text.
         */
        [[nodiscard]] static ReadOnlySpan<char> AsSpan(const std::string& text) {
            return ReadOnlySpan<char>(text.data(),
                                     static_cast<SharpRuntime::intcs>(text.size()));
        }

        /**
         * @brief Creates a new ReadOnlySpan<char> over the string starting at @p start.
         *
         * C++ counterpart of .NET MemoryExtensions.AsSpan(string, int).
         * @param text  The source string.
         * @param start The index at which to begin this slice.
         * @throws std::out_of_range if @p start is out of bounds.
         */
        [[nodiscard]] static ReadOnlySpan<char> AsSpan(const std::string& text,
                                                        SharpRuntime::intcs start) {
            auto sz = static_cast<SharpRuntime::intcs>(text.size());
            if (start < 0 || start > sz)
                throw std::out_of_range("start is out of range.");
            return ReadOnlySpan<char>(text.data() + start, sz - start);
        }

        /**
         * @brief Creates a new ReadOnlySpan<char> over a substring of @p text.
         *
         * C++ counterpart of .NET MemoryExtensions.AsSpan(string, int, int).
         * @param text   The source string.
         * @param start  The index at which to begin this slice.
         * @param length The desired length of the slice.
         * @throws std::out_of_range if @p start or @p length is out of bounds.
         */
        [[nodiscard]] static ReadOnlySpan<char> AsSpan(const std::string& text,
                                                        SharpRuntime::intcs start,
                                                        SharpRuntime::intcs length) {
            auto sz = static_cast<SharpRuntime::intcs>(text.size());
            if (start < 0 || length < 0 || start + length > sz)
                throw std::out_of_range("start or length is out of range.");
            return ReadOnlySpan<char>(text.data() + start, length);
        }

        // ---------------------------------------------------------------
        // Contains
        // ---------------------------------------------------------------

        /**
         * @brief Returns true if @p value is found in the span.
         *
         * C++ counterpart of .NET MemoryExtensions.Contains<T>(ReadOnlySpan<T>, T).
         */
        template<typename T>
        [[nodiscard]] static bool Contains(ReadOnlySpan<T> span, const T& value) {
            for (SharpRuntime::intcs i = 0; i < span.getLengthProperty(); ++i)
                if (span[i] == value) return true;
            return false;
        }

        template<typename T>
        [[nodiscard]] static bool Contains(Span<T> span, const T& value) {
            return Contains(ReadOnlySpan<T>(span.getPointer(), span.getLengthProperty()), value);
        }

        // ---------------------------------------------------------------
        // ContainsAny
        // ---------------------------------------------------------------

        /**
         * @brief Returns true if @p value0 or @p value1 is found in the span.
         *
         * C++ counterpart of .NET MemoryExtensions.ContainsAny<T>(ReadOnlySpan<T>, T, T).
         */
        template<typename T>
        [[nodiscard]] static bool ContainsAny(ReadOnlySpan<T> span,
                                               const T& value0, const T& value1) {
            for (SharpRuntime::intcs i = 0; i < span.getLengthProperty(); ++i)
                if (span[i] == value0 || span[i] == value1) return true;
            return false;
        }

        /**
         * @brief Returns true if any of @p value0, @p value1, or @p value2 is found.
         *
         * C++ counterpart of .NET MemoryExtensions.ContainsAny<T>(ReadOnlySpan<T>, T, T, T).
         */
        template<typename T>
        [[nodiscard]] static bool ContainsAny(ReadOnlySpan<T> span,
                                               const T& value0, const T& value1, const T& value2) {
            for (SharpRuntime::intcs i = 0; i < span.getLengthProperty(); ++i)
                if (span[i] == value0 || span[i] == value1 || span[i] == value2) return true;
            return false;
        }

        /**
         * @brief Returns true if any element of @p values is found in @p span.
         *
         * C++ counterpart of .NET MemoryExtensions.ContainsAny<T>(ReadOnlySpan<T>, ReadOnlySpan<T>).
         */
        template<typename T>
        [[nodiscard]] static bool ContainsAny(ReadOnlySpan<T> span, ReadOnlySpan<T> values) {
            for (SharpRuntime::intcs i = 0; i < span.getLengthProperty(); ++i)
                for (SharpRuntime::intcs j = 0; j < values.getLengthProperty(); ++j)
                    if (span[i] == values[j]) return true;
            return false;
        }

        // ---------------------------------------------------------------
        // IndexOf — single element
        // ---------------------------------------------------------------

        /**
         * @brief Returns the index of the first occurrence of @p value, or -1 if not found.
         *
         * C++ counterpart of .NET MemoryExtensions.IndexOf<T>(ReadOnlySpan<T>, T).
         */
        template<typename T>
        [[nodiscard]] static SharpRuntime::intcs IndexOf(ReadOnlySpan<T> span, const T& value) {
            for (SharpRuntime::intcs i = 0; i < span.getLengthProperty(); ++i)
                if (span[i] == value) return i;
            return -1;
        }

        template<typename T>
        [[nodiscard]] static SharpRuntime::intcs IndexOf(Span<T> span, const T& value) {
            return IndexOf(ReadOnlySpan<T>(span.getPointer(), span.getLengthProperty()), value);
        }

        // ---------------------------------------------------------------
        // IndexOf — subsequence
        // ---------------------------------------------------------------

        /**
         * @brief Returns the index of the first occurrence of @p value as a sub-span, or -1.
         *
         * C++ counterpart of .NET MemoryExtensions.IndexOf<T>(ReadOnlySpan<T>, ReadOnlySpan<T>).
         */
        template<typename T>
        [[nodiscard]] static SharpRuntime::intcs IndexOf(ReadOnlySpan<T> span,
                                                          ReadOnlySpan<T> value) {
            SharpRuntime::intcs sLen = span.getLengthProperty();
            SharpRuntime::intcs vLen = value.getLengthProperty();
            if (vLen == 0) return 0;
            if (vLen > sLen) return -1;
            for (SharpRuntime::intcs i = 0; i <= sLen - vLen; ++i) {
                bool match = true;
                for (SharpRuntime::intcs j = 0; j < vLen && match; ++j)
                    if (span[i + j] != value[j]) match = false;
                if (match) return i;
            }
            return -1;
        }

        // ---------------------------------------------------------------
        // LastIndexOf — single element
        // ---------------------------------------------------------------

        /**
         * @brief Returns the index of the last occurrence of @p value, or -1 if not found.
         *
         * C++ counterpart of .NET MemoryExtensions.LastIndexOf<T>(ReadOnlySpan<T>, T).
         */
        template<typename T>
        [[nodiscard]] static SharpRuntime::intcs LastIndexOf(ReadOnlySpan<T> span, const T& value) {
            for (SharpRuntime::intcs i = span.getLengthProperty() - 1; i >= 0; --i)
                if (span[i] == value) return i;
            return -1;
        }

        template<typename T>
        [[nodiscard]] static SharpRuntime::intcs LastIndexOf(Span<T> span, const T& value) {
            return LastIndexOf(ReadOnlySpan<T>(span.getPointer(), span.getLengthProperty()), value);
        }

        // ---------------------------------------------------------------
        // LastIndexOf — subsequence
        // ---------------------------------------------------------------

        /**
         * @brief Returns the index of the last occurrence of @p value as a sub-span, or -1.
         *
         * C++ counterpart of .NET MemoryExtensions.LastIndexOf<T>(ReadOnlySpan<T>, ReadOnlySpan<T>).
         */
        template<typename T>
        [[nodiscard]] static SharpRuntime::intcs LastIndexOf(ReadOnlySpan<T> span,
                                                              ReadOnlySpan<T> value) {
            SharpRuntime::intcs sLen = span.getLengthProperty();
            SharpRuntime::intcs vLen = value.getLengthProperty();
            if (vLen == 0) return sLen;
            if (vLen > sLen) return -1;
            for (SharpRuntime::intcs i = sLen - vLen; i >= 0; --i) {
                bool match = true;
                for (SharpRuntime::intcs j = 0; j < vLen && match; ++j)
                    if (span[i + j] != value[j]) match = false;
                if (match) return i;
            }
            return -1;
        }

        // ---------------------------------------------------------------
        // SequenceEqual
        // ---------------------------------------------------------------

        /**
         * @brief Returns true if both spans have the same length and elements.
         *
         * C++ counterpart of .NET MemoryExtensions.SequenceEqual<T>(ReadOnlySpan<T>, ReadOnlySpan<T>).
         */
        template<typename T>
        [[nodiscard]] static bool SequenceEqual(ReadOnlySpan<T> a, ReadOnlySpan<T> b) {
            if (a.getLengthProperty() != b.getLengthProperty()) return false;
            for (SharpRuntime::intcs i = 0; i < a.getLengthProperty(); ++i)
                if (a[i] != b[i]) return false;
            return true;
        }

        template<typename T>
        [[nodiscard]] static bool SequenceEqual(Span<T> a, Span<T> b) {
            return SequenceEqual(
                ReadOnlySpan<T>(a.getPointer(), a.getLengthProperty()),
                ReadOnlySpan<T>(b.getPointer(), b.getLengthProperty()));
        }

        // ---------------------------------------------------------------
        // SequenceCompareTo
        // ---------------------------------------------------------------

        /**
         * @brief Compares two spans element by element and returns an integer indicating order.
         *
         * C++ counterpart of .NET MemoryExtensions.SequenceCompareTo<T>(ReadOnlySpan<T>, ReadOnlySpan<T>).
         * @return Negative if @p a < @p b, zero if equal, positive if @p a > @p b.
         */
        template<typename T>
        [[nodiscard]] static SharpRuntime::intcs SequenceCompareTo(ReadOnlySpan<T> a,
                                                                    ReadOnlySpan<T> b) {
            SharpRuntime::intcs minLen = std::min(a.getLengthProperty(), b.getLengthProperty());
            for (SharpRuntime::intcs i = 0; i < minLen; ++i) {
                if (a[i] < b[i]) return -1;
                if (a[i] > b[i]) return  1;
            }
            return a.getLengthProperty() - b.getLengthProperty();
        }

        // ---------------------------------------------------------------
        // Count
        // ---------------------------------------------------------------

        /**
         * @brief Counts the number of occurrences of @p value in the span.
         *
         * C++ counterpart of .NET MemoryExtensions.Count<T>(ReadOnlySpan<T>, T).
         */
        template<typename T>
        [[nodiscard]] static SharpRuntime::intcs Count(ReadOnlySpan<T> span, const T& value) {
            SharpRuntime::intcs n = 0;
            for (SharpRuntime::intcs i = 0; i < span.getLengthProperty(); ++i)
                if (span[i] == value) ++n;
            return n;
        }

        template<typename T>
        [[nodiscard]] static SharpRuntime::intcs Count(Span<T> span, const T& value) {
            return Count(ReadOnlySpan<T>(span.getPointer(), span.getLengthProperty()), value);
        }

        // ---------------------------------------------------------------
        // Fill
        // ---------------------------------------------------------------

        /**
         * @brief Fills every element of the span with @p value.
         *
         * C++ counterpart of .NET MemoryExtensions.Fill<T>(Span<T>, T).
         */
        template<typename T>
        static void Fill(Span<T> span, const T& value) {
            std::fill(span.begin(), span.end(), value);
        }

        // ---------------------------------------------------------------
        // Reverse
        // ---------------------------------------------------------------

        /**
         * @brief Reverses the elements of the span in place.
         *
         * C++ counterpart of .NET MemoryExtensions.Reverse<T>(Span<T>).
         */
        template<typename T>
        static void Reverse(Span<T> span) {
            std::reverse(span.begin(), span.end());
        }

        // ---------------------------------------------------------------
        // CopyTo
        // ---------------------------------------------------------------

        /**
         * @brief Copies the contents of @p source into @p destination.
         *
         * C++ counterpart of .NET MemoryExtensions.CopyTo<T>(ReadOnlySpan<T>, Span<T>).
         */
        template<typename T>
        static void CopyTo(ReadOnlySpan<T> source, Span<T> destination) {
            std::copy(source.begin(), source.end(), destination.begin());
        }

        template<typename T>
        static void CopyTo(Span<T> source, Span<T> destination) {
            CopyTo(ReadOnlySpan<T>(source.getPointer(), source.getLengthProperty()), destination);
        }

        // ---------------------------------------------------------------
        // Sort
        // ---------------------------------------------------------------

        /**
         * @brief Sorts the elements of the span using operator<.
         *
         * C++ counterpart of .NET MemoryExtensions.Sort<T>(Span<T>).
         */
        template<typename T>
        static void Sort(Span<T> span) {
            std::sort(span.begin(), span.end());
        }

        /**
         * @brief Sorts the elements of the span using a custom comparator.
         *
         * C++ counterpart of .NET MemoryExtensions.Sort<T, TComparer>(Span<T>, TComparer).
         */
        template<typename T, typename TComparer>
        static void Sort(Span<T> span, TComparer comparer) {
            std::sort(span.begin(), span.end(), comparer);
        }

        // ---------------------------------------------------------------
        // BinarySearch
        // ---------------------------------------------------------------

        /**
         * @brief Searches for @p value in a sorted span using binary search.
         *
         * C++ counterpart of .NET MemoryExtensions.BinarySearch<T>(ReadOnlySpan<T>, T).
         * @return The index of the found element, or a negative bitwise-complement index
         *         indicating where the value would be inserted (matching .NET convention).
         */
        template<typename T>
        [[nodiscard]] static SharpRuntime::intcs BinarySearch(ReadOnlySpan<T> span, const T& value) {
            SharpRuntime::intcs lo = 0, hi = span.getLengthProperty() - 1;
            while (lo <= hi) {
                SharpRuntime::intcs mid = lo + (hi - lo) / 2;
                if (span[mid] == value) return mid;
                if (span[mid] < value) lo = mid + 1;
                else                   hi = mid - 1;
            }
            return ~lo;
        }

        template<typename T>
        [[nodiscard]] static SharpRuntime::intcs BinarySearch(Span<T> span, const T& value) {
            return BinarySearch(ReadOnlySpan<T>(span.getPointer(), span.getLengthProperty()), value);
        }

        // ---------------------------------------------------------------
        // StartsWith / EndsWith
        // ---------------------------------------------------------------

        /**
         * @brief Returns true if @p span starts with the elements of @p value.
         *
         * C++ counterpart of .NET MemoryExtensions.StartsWith<T>(ReadOnlySpan<T>, ReadOnlySpan<T>).
         */
        template<typename T>
        [[nodiscard]] static bool StartsWith(ReadOnlySpan<T> span, ReadOnlySpan<T> value) {
            if (value.getLengthProperty() > span.getLengthProperty()) return false;
            return SequenceEqual(span.Slice(0, value.getLengthProperty()), value);
        }

        /**
         * @brief Returns true if @p span ends with the elements of @p value.
         *
         * C++ counterpart of .NET MemoryExtensions.EndsWith<T>(ReadOnlySpan<T>, ReadOnlySpan<T>).
         */
        template<typename T>
        [[nodiscard]] static bool EndsWith(ReadOnlySpan<T> span, ReadOnlySpan<T> value) {
            SharpRuntime::intcs vLen = value.getLengthProperty();
            SharpRuntime::intcs sLen = span.getLengthProperty();
            if (vLen > sLen) return false;
            return SequenceEqual(span.Slice(sLen - vLen, vLen), value);
        }

        // ---------------------------------------------------------------
        // Overlaps
        // ---------------------------------------------------------------

        /**
         * @brief Determines whether @p span and @p other overlap in memory.
         *
         * C++ counterpart of .NET MemoryExtensions.Overlaps<T>(ReadOnlySpan<T>, ReadOnlySpan<T>).
         */
        template<typename T>
        [[nodiscard]] static bool Overlaps(ReadOnlySpan<T> span, ReadOnlySpan<T> other) {
            if (span.getLengthProperty() == 0 || other.getLengthProperty() == 0) return false;
            const T* s = span.getPointer();
            const T* o = other.getPointer();
            return s < o + other.getLengthProperty() && o < s + span.getLengthProperty();
        }

        template<typename T>
        [[nodiscard]] static bool Overlaps(Span<T> span, Span<T> other) {
            return Overlaps(
                ReadOnlySpan<T>(span.getPointer(), span.getLengthProperty()),
                ReadOnlySpan<T>(other.getPointer(), other.getLengthProperty()));
        }

        // ---------------------------------------------------------------
        // Trim / TrimStart / TrimEnd — whitespace (char spans only)
        // ---------------------------------------------------------------

        /**
         * @brief Removes whitespace from both ends of a char span.
         *
         * C++ counterpart of .NET MemoryExtensions.Trim(ReadOnlySpan<char>).
         */
        [[nodiscard]] static ReadOnlySpan<char> Trim(ReadOnlySpan<char> span) {
            return TrimEnd(TrimStart(span));
        }

        /**
         * @brief Removes leading whitespace from a char span.
         *
         * C++ counterpart of .NET MemoryExtensions.TrimStart(ReadOnlySpan<char>).
         */
        [[nodiscard]] static ReadOnlySpan<char> TrimStart(ReadOnlySpan<char> span) {
            SharpRuntime::intcs i = 0;
            while (i < span.getLengthProperty() &&
                   std::isspace(static_cast<unsigned char>(span[i]))) ++i;
            return span.Slice(i);
        }

        /**
         * @brief Removes trailing whitespace from a char span.
         *
         * C++ counterpart of .NET MemoryExtensions.TrimEnd(ReadOnlySpan<char>).
         */
        [[nodiscard]] static ReadOnlySpan<char> TrimEnd(ReadOnlySpan<char> span) {
            SharpRuntime::intcs i = span.getLengthProperty() - 1;
            while (i >= 0 && std::isspace(static_cast<unsigned char>(span[i]))) --i;
            return span.Slice(0, i + 1);
        }

        // ---------------------------------------------------------------
        // Trim / TrimStart / TrimEnd — single trim element (generic)
        // ---------------------------------------------------------------

        /**
         * @brief Removes all occurrences of @p trimElement from both ends of the span.
         *
         * C++ counterpart of .NET MemoryExtensions.Trim<T>(ReadOnlySpan<T>, T).
         */
        template<typename T>
        [[nodiscard]] static ReadOnlySpan<T> Trim(ReadOnlySpan<T> span, const T& trimElement) {
            return TrimEnd(TrimStart(span, trimElement), trimElement);
        }

        /**
         * @brief Removes all leading occurrences of @p trimElement from the span.
         *
         * C++ counterpart of .NET MemoryExtensions.TrimStart<T>(ReadOnlySpan<T>, T).
         */
        template<typename T>
        [[nodiscard]] static ReadOnlySpan<T> TrimStart(ReadOnlySpan<T> span,
                                                         const T& trimElement) {
            SharpRuntime::intcs i = 0;
            while (i < span.getLengthProperty() && span[i] == trimElement) ++i;
            return span.Slice(i);
        }

        /**
         * @brief Removes all trailing occurrences of @p trimElement from the span.
         *
         * C++ counterpart of .NET MemoryExtensions.TrimEnd<T>(ReadOnlySpan<T>, T).
         */
        template<typename T>
        [[nodiscard]] static ReadOnlySpan<T> TrimEnd(ReadOnlySpan<T> span,
                                                       const T& trimElement) {
            SharpRuntime::intcs i = span.getLengthProperty() - 1;
            while (i >= 0 && span[i] == trimElement) --i;
            return span.Slice(0, i + 1);
        }
    };

} // namespace System
