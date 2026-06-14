// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <vector>
#include "System/Span.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {

    /**
     * @brief Extension-like static methods for Span<T> and ReadOnlySpan<T>.
     *
     * Partial C++ counterpart of .NET System.MemoryExtensions.
     * Covers the subset most relevant to game-dev / XNA porting:
     * AsSpan, Contains, IndexOf, LastIndexOf, SequenceEqual,
     * Fill, Reverse, CopyTo, Sort, StartsWith, EndsWith.
     */
    struct MemoryExtensions {
        MemoryExtensions() = delete;

        // ---------------------------------------------------------------
        // AsSpan — create a Span over a subrange of a vector
        // ---------------------------------------------------------------

        /** @brief Returns a Span over the entire vector. */
        template<typename T>
        [[nodiscard]] static Span<T> AsSpan(std::vector<T>& v) {
            return Span<T>(v);
        }

        /** @brief Returns a Span over vector elements starting at start. */
        template<typename T>
        [[nodiscard]] static Span<T> AsSpan(std::vector<T>& v, SharpRuntime::intcs start) {
            return Span<T>(v.data() + start, static_cast<SharpRuntime::intcs>(v.size()) - start);
        }

        /** @brief Returns a Span over length elements of vector starting at start. */
        template<typename T>
        [[nodiscard]] static Span<T> AsSpan(std::vector<T>& v, SharpRuntime::intcs start,
                                             SharpRuntime::intcs length) {
            return Span<T>(v.data() + start, length);
        }

        /** @brief Returns a ReadOnlySpan over the entire vector. */
        template<typename T>
        [[nodiscard]] static ReadOnlySpan<T> AsReadOnlySpan(const std::vector<T>& v) {
            return ReadOnlySpan<T>(v);
        }

        // ---------------------------------------------------------------
        // Contains
        // ---------------------------------------------------------------

        /** @brief Returns true if value is found in the span. */
        template<typename T>
        [[nodiscard]] static bool Contains(ReadOnlySpan<T> span, const T& value) {
            for (auto& e : span) if (e == value) return true;
            return false;
        }

        template<typename T>
        [[nodiscard]] static bool Contains(Span<T> span, const T& value) {
            return Contains(ReadOnlySpan<T>(span.getPointer(), span.getLengthProperty()), value);
        }

        // ---------------------------------------------------------------
        // IndexOf
        // ---------------------------------------------------------------

        /** @brief Returns the index of the first occurrence of value, or -1 if not found. */
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
        // LastIndexOf
        // ---------------------------------------------------------------

        /** @brief Returns the index of the last occurrence of value, or -1 if not found. */
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
        // SequenceEqual
        // ---------------------------------------------------------------

        /** @brief Returns true if both spans contain the same elements in the same order. */
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
        // Fill
        // ---------------------------------------------------------------

        /** @brief Fills every element of the span with value. */
        template<typename T>
        static void Fill(Span<T> span, const T& value) {
            std::fill(span.begin(), span.end(), value);
        }

        // ---------------------------------------------------------------
        // Reverse
        // ---------------------------------------------------------------

        /** @brief Reverses the elements of the span in place. */
        template<typename T>
        static void Reverse(Span<T> span) {
            std::reverse(span.begin(), span.end());
        }

        // ---------------------------------------------------------------
        // CopyTo
        // ---------------------------------------------------------------

        /** @brief Copies the contents of source into destination. */
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

        /** @brief Sorts the elements of the span using operator<. */
        template<typename T>
        static void Sort(Span<T> span) {
            std::sort(span.begin(), span.end());
        }

        /** @brief Sorts the elements of the span using a custom comparator. */
        template<typename T, typename TComparer>
        static void Sort(Span<T> span, TComparer comparer) {
            std::sort(span.begin(), span.end(), comparer);
        }

        // ---------------------------------------------------------------
        // StartsWith / EndsWith
        // ---------------------------------------------------------------

        /** @brief Returns true if span starts with the elements of value. */
        template<typename T>
        [[nodiscard]] static bool StartsWith(ReadOnlySpan<T> span, ReadOnlySpan<T> value) {
            if (value.getLengthProperty() > span.getLengthProperty()) return false;
            return SequenceEqual(span.Slice(0, value.getLengthProperty()), value);
        }

        /** @brief Returns true if span ends with the elements of value. */
        template<typename T>
        [[nodiscard]] static bool EndsWith(ReadOnlySpan<T> span, ReadOnlySpan<T> value) {
            SharpRuntime::intcs vLen = value.getLengthProperty();
            SharpRuntime::intcs sLen = span.getLengthProperty();
            if (vLen > sLen) return false;
            return SequenceEqual(span.Slice(sLen - vLen, vLen), value);
        }
    };

} // namespace System
