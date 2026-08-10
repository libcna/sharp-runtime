// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <unordered_set>
#include <vector>
#include "System/Span.hpp"
#include "System/ReadOnlySpan.hpp"

namespace System::Buffers {

namespace detail {
    /** @brief True when `std::hash<T>` is usable — default-constructible and callable. */
    template<typename T>
    inline constexpr bool searchValuesHashUsable =
        requires(const T& value) { std::hash<T>{}(value); };

    /** @brief True when two `const T&` can be compared with `operator==`. */
    template<typename T>
    inline constexpr bool searchValuesEqualityUsable =
        requires(const T& a, const T& b) { a == b; };
} // namespace detail

/**
 * @brief Provides an immutable, read-only set of values optimized for searching.
 *
 * C++ counterpart of .NET System.Buffers.SearchValues&lt;T&gt;.
 * Instances are created via the SearchValues factory (see Create overloads below).
 * This implementation uses an unordered_set for O(1) Contains() lookups.
 *
 * @tparam T The type of the values to search for.
 *
 * @par Requirements on T
 * .NET's `SearchValues<T>` requires only `IEquatable<T>`. This port stores the values in a
 * `std::unordered_set<T>`, so it requires **more**: `T` must be equality-comparable **and**
 * must have a usable `std::hash<T>` specialization. An equality-only type — the exact shape
 * .NET's own constraint admits — is rejected by this port; supply a `std::hash<T>`
 * specialization for it. Naming or declaring `SearchValues<T>` stays legal for any `T`; the
 * requirement applies to the two constructors, which is where the set is populated.
 *
 * Both halves are `static_assert`ed where they are already enforced, so the same set of
 * programs compiles as before and only the diagnostic changes.
 * Ticket #2054 / SR-AUD-077, family B-C; see docs/BuffersNamespaceReviewPlan.md §4.4.
 */
template<typename T>
class SearchValues {
    std::unordered_set<T> values_;

public:
    // The two asserts below are deliberately in the CONSTRUCTOR bodies, not at class scope:
    // `SearchValues<T>` can be named and implicitly instantiated today for a type with no
    // usable `std::hash<T>` (the member declaration alone compiles), so a class-scope assert
    // would reject a program that only names the type. They are duplicated rather than
    // factored into a macro, because a macro in a public header is worse. Ticket #2054.

    /**
     * @brief Constructs a SearchValues from an initializer list of values.
     * @note Requires an equality-comparable T with a usable `std::hash<T>`; see the
     *       class-level "Requirements on T" note.
     */
    SearchValues(std::initializer_list<T> values) : values_(values) {
        static_assert(
            detail::searchValuesEqualityUsable<T>,
            "System::Buffers::SearchValues<T> requires T to be equality-comparable: the "
            "backing std::unordered_set<T> compares elements with operator==. "
            "See docs/BuffersNamespaceReviewPlan.md, ticket #2054.");
        static_assert(
            detail::searchValuesHashUsable<T>,
            "System::Buffers::SearchValues<T> requires a usable std::hash<T>: the backing "
            "std::unordered_set<T> hashes every element. .NET's SearchValues<T> needs only "
            "equality, so an equality-only T must be given a std::hash<T> specialization "
            "for this port. See docs/BuffersNamespaceReviewPlan.md, ticket #2054.");
    }

    /**
     * @brief Constructs a SearchValues from a vector of values.
     * @note Requires an equality-comparable T with a usable `std::hash<T>`; see the
     *       class-level "Requirements on T" note.
     */
    explicit SearchValues(const std::vector<T>& values)
        : values_(values.begin(), values.end()) {
        static_assert(
            detail::searchValuesEqualityUsable<T>,
            "System::Buffers::SearchValues<T> requires T to be equality-comparable: the "
            "backing std::unordered_set<T> compares elements with operator==. "
            "See docs/BuffersNamespaceReviewPlan.md, ticket #2054.");
        static_assert(
            detail::searchValuesHashUsable<T>,
            "System::Buffers::SearchValues<T> requires a usable std::hash<T>: the backing "
            "std::unordered_set<T> hashes every element. .NET's SearchValues<T> needs only "
            "equality, so an equality-only T must be given a std::hash<T> specialization "
            "for this port. See docs/BuffersNamespaceReviewPlan.md, ticket #2054.");
    }

    /**
     * @brief Searches for the specified value and returns true if found.
     *
     * C++ counterpart of .NET SearchValues&lt;T&gt;.Contains(T).
     * @param value The value to search for.
     * @return true if @p value is in the set; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& value) const {
        return values_.count(value) > 0;
    }

    /**
     * @brief Returns a copy of all values in this SearchValues instance.
     *
     * C++ counterpart of the internal .NET SearchValues&lt;T&gt;.GetValues().
     */
    [[nodiscard]] std::vector<T> GetValues() const {
        return std::vector<T>(values_.begin(), values_.end());
    }
};

/**
 * @brief Factory class for creating SearchValues instances.
 *
 * C++ counterpart of .NET System.Buffers.SearchValues (the static factory class).
 */
class SearchValuesFactory {
public:
    SearchValuesFactory() = delete;

    /**
     * @brief Creates an optimized SearchValues&lt;uint8_t&gt; from the given values.
     *
     * C++ counterpart of .NET SearchValues.Create(ReadOnlySpan&lt;byte&gt;).
     */
    [[nodiscard]] static SearchValues<uint8_t> Create(
        std::initializer_list<uint8_t> values)
    {
        return SearchValues<uint8_t>(values);
    }

    /**
     * @brief Creates an optimized SearchValues&lt;char&gt; from the given values.
     *
     * C++ counterpart of .NET SearchValues.Create(ReadOnlySpan&lt;char&gt;).
     */
    [[nodiscard]] static SearchValues<char> Create(
        std::initializer_list<char> values)
    {
        return SearchValues<char>(values);
    }

    /**
     * @brief Creates a SearchValues&lt;T&gt; from the given vector.
     *
     * Generic overload for any equality-comparable type T.
     */
    template<typename T>
    [[nodiscard]] static SearchValues<T> Create(const std::vector<T>& values) {
        return SearchValues<T>(values);
    }
};

} // namespace System::Buffers
