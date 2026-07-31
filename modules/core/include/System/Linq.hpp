// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/detail/ComparisonPolicy.hpp"

namespace System::Linq {

    using SharpRuntime::intcs;

    /**
     * @brief Header-only LINQ-style sequence operators over std::vector<T>.
     *
     * Partial C++ counterpart of LINQ methods from System.Linq.Enumerable.
     * All operators take a std::vector<T> by value and return a new std::vector.
     *
     * Implemented: Where, Select, FirstOrDefault, First, LastOrDefault, Any, All, Count,
     * ToList, Sum, Min, Max, OrderBy, OrderByDescending, Distinct, Reverse, Skip, Take,
     * Concat, Contains. Not implemented (real .NET's Enumerable surface is 100+ methods):
     * GroupBy, Join, Zip, SelectMany, ToDictionary, ToHashSet, Average, Aggregate, ElementAt,
     * SkipWhile, TakeWhile, Except, Intersect, Union, SequenceEqual, Single/SingleOrDefault,
     * DefaultIfEmpty, Chunk, and more -- add on demand as ported code needs them, per
     * CLAUDE.md's "No LINQ" policy for code THIS project writes (this class exists only to
     * support already-ported C#/XNA call sites that use these operators).
     *
     * @note Status: PARTIAL
     */

    namespace detail {

        /**
         * @brief Rejects an empty callback at the public boundary, before the sequence.
         *
         * Every operator below stored its callable as a `std::function` and never validated
         * it, so an empty one made failure *data-dependent*: an empty sequence returned an
         * ordinary result, `OrderBy`/`OrderByDescending` did so for a one-element sequence
         * too (`std::stable_sort` never compares below two), and anything larger reached
         * `std::bad_function_call` -- a native exception outside the System::Exception
         * hierarchy, which ported `catch (const Exception&)` code cannot see. Current .NET
         * rejects a null delegate at the argument boundary before examining the sequence, and
         * does so eagerly even for the deferred operators: `OrderBy`'s `keySelector` check
         * lives in `OrderedIterator`'s constructor (`OrderedEnumerable.cs:80-82`), which runs
         * before any element is touched. These operators are eager throughout, so an eager
         * check is an exact match rather than an approximation.
         *
         * Ticket #1870 / SR-AUD-134 / CCF-011; see docs/EmptyCallableBoundaryPlan.md.
         *
         * @param callable  The callback to validate.
         * @param paramName The .NET parameter name to carry in the message.
         * @throws System::ArgumentNullException if @p callable is empty.
         */
        template<typename Fn>
        inline void requireCallable(const Fn& callable, const char* paramName) {
            if (!callable) throw System::ArgumentNullException(paramName);
        }

    } // namespace detail

    /**
     * @brief Returns elements that satisfy @p predicate.
     * @throws System::ArgumentNullException if @p predicate is an empty std::function.
     */
    template<typename T>
    std::vector<T> Where(const std::vector<T>& source,
                         std::function<bool(const T&)> predicate)
    {
        detail::requireCallable(predicate, "predicate");
        std::vector<T> result;
        for (const auto& item : source)
            if (predicate(item)) result.push_back(item);
        return result;
    }

    /**
     * @brief Projects each element using @p selector.
     * @throws System::ArgumentNullException if @p selector is an empty std::function.
     */
    template<typename T, typename R>
    std::vector<R> Select(const std::vector<T>& source,
                          std::function<R(const T&)> selector)
    {
        detail::requireCallable(selector, "selector");
        std::vector<R> result;
        result.reserve(source.size());
        for (const auto& item : source)
            result.push_back(selector(item));
        return result;
    }

    /**
     * @brief Returns the first element matching @p predicate, or default T{} if none.
     * @throws System::ArgumentNullException if @p predicate is an empty std::function.
     */
    template<typename T>
    T FirstOrDefault(const std::vector<T>& source,
                     std::function<bool(const T&)> predicate)
    {
        detail::requireCallable(predicate, "predicate");
        for (const auto& item : source)
            if (predicate(item)) return item;
        return T{};
    }

    /** @brief Returns the first element, or default T{} if the sequence is empty. */
    template<typename T>
    T FirstOrDefault(const std::vector<T>& source)
    {
        return source.empty() ? T{} : source.front();
    }

    /**
     * @brief Returns the first element matching @p predicate; throws if none found.
     * @throws System::ArgumentNullException if @p predicate is an empty std::function. The
     *         argument error is raised **before** the sequence is examined, so an empty
     *         sequence reports it rather than the InvalidOperationException below --
     *         matching .NET's First(source, predicate) (First.cs:110).
     * @throws System::InvalidOperationException if no element matches.
     */
    template<typename T>
    T First(const std::vector<T>& source,
            std::function<bool(const T&)> predicate)
    {
        detail::requireCallable(predicate, "predicate");
        for (const auto& item : source)
            if (predicate(item)) return item;
        throw System::InvalidOperationException("Sequence contains no matching element.");
    }

    /** @brief Returns the first element; throws if the sequence is empty. */
    template<typename T>
    T First(const std::vector<T>& source)
    {
        if (source.empty()) throw System::InvalidOperationException("Sequence contains no elements.");
        return source.front();
    }

    /**
     * @brief Returns the last element matching @p predicate, or default T{} if none.
     * @throws System::ArgumentNullException if @p predicate is an empty std::function.
     */
    template<typename T>
    T LastOrDefault(const std::vector<T>& source,
                    std::function<bool(const T&)> predicate)
    {
        detail::requireCallable(predicate, "predicate");
        T result{};
        bool found = false;
        for (const auto& item : source)
            if (predicate(item)) { result = item; found = true; }
        (void)found;
        return result;
    }

    /** @brief Returns the last element, or default T{} if empty. */
    template<typename T>
    T LastOrDefault(const std::vector<T>& source)
    {
        return source.empty() ? T{} : source.back();
    }

    /**
     * @brief Returns true if any element satisfies @p predicate.
     * @throws System::ArgumentNullException if @p predicate is an empty std::function.
     */
    template<typename T>
    bool Any(const std::vector<T>& source,
             std::function<bool(const T&)> predicate)
    {
        detail::requireCallable(predicate, "predicate");
        for (const auto& item : source)
            if (predicate(item)) return true;
        return false;
    }

    /** @brief Returns true if the sequence contains at least one element. */
    template<typename T>
    bool Any(const std::vector<T>& source) { return !source.empty(); }

    /**
     * @brief Returns true if all elements satisfy @p predicate (true for empty sequences).
     * @throws System::ArgumentNullException if @p predicate is an empty std::function --
     *         including for an empty sequence, which used to return true without examining it.
     */
    template<typename T>
    bool All(const std::vector<T>& source,
             std::function<bool(const T&)> predicate)
    {
        detail::requireCallable(predicate, "predicate");
        for (const auto& item : source)
            if (!predicate(item)) return false;
        return true;
    }

    /**
     * @brief Returns the number of elements satisfying @p predicate.
     * @throws System::ArgumentNullException if @p predicate is an empty std::function.
     */
    template<typename T>
    intcs Count(const std::vector<T>& source,
                std::function<bool(const T&)> predicate)
    {
        detail::requireCallable(predicate, "predicate");
        intcs n = 0;
        for (const auto& item : source)
            if (predicate(item)) ++n;
        return n;
    }

    /** @brief Returns the total number of elements. */
    template<typename T>
    intcs Count(const std::vector<T>& source)
    {
        return static_cast<intcs>(source.size());
    }

    /** @brief Returns the input sequence as a vector (identity; mirrors .NET ToList()). */
    template<typename T>
    std::vector<T> ToList(const std::vector<T>& source) { return source; }

    /** @brief Returns the sum of all elements (requires operator+). */
    template<typename T>
    T Sum(const std::vector<T>& source)
    {
        T result{};
        for (const auto& item : source) result = result + item;
        return result;
    }

    /**
     * @brief Returns the sum of a projected value (requires operator+).
     * @throws System::ArgumentNullException if @p selector is an empty std::function.
     */
    template<typename T, typename R>
    R Sum(const std::vector<T>& source, std::function<R(const T&)> selector)
    {
        detail::requireCallable(selector, "selector");
        R result{};
        for (const auto& item : source) result = result + selector(item);
        return result;
    }

    /**
     * @brief Returns the minimum element under .NET's default comparison contract.
     *
     * C++ counterpart of .NET Enumerable.Min. For `float` and `double` the reference
     * imposes a total ordering in which NaN is smaller than every value including
     * negative infinity, and says so in a comment at `Linq/Min.cs:130-139`: it
     * short-circuits and returns the first NaN it meets, so `Min({1,2,NaN})` is NaN.
     * `std::min_element` with raw `<` returned 1.
     *
     * @note Min and Max are deliberately NOT symmetric — see Max below. Both come
     * from the same rule applied to opposite ends of the ordering.
     * @throws System::InvalidOperationException if @p source is empty.
     */
    template<typename T>
    T Min(const std::vector<T>& source)
    {
        if (source.empty()) throw System::InvalidOperationException("Sequence contains no elements.");
        if constexpr (std::is_floating_point_v<T>) {
            T value = source[0];
            if (std::isnan(value)) return value;
            for (std::size_t i = 1; i < source.size(); ++i) {
                const T x = source[i];
                if (x < value) value = x;
                else if (std::isnan(x)) return x;   // nothing can be smaller
            }
            return value;
        } else {
            return *std::min_element(source.begin(), source.end());
        }
    }

    /**
     * @brief Returns the maximum element under .NET's default comparison contract.
     *
     * C++ counterpart of .NET Enumerable.Max. Because NaN is the smallest value in
     * the reference's ordering, `MaxFloat` (`Linq/Max.cs:96-130`) SKIPS every leading
     * NaN and then compares ordinarily, so `Max({NaN,1,2})` is 2 — where
     * `std::max_element` with raw `<` returned NaN. An all-NaN sequence returns its
     * last element, matching the reference's `return span[^1];`.
     *
     * @note This is the opposite direction of adjustment from Min above, not the
     * same one; making the two symmetric would break one of them.
     * @throws System::InvalidOperationException if @p source is empty.
     */
    template<typename T>
    T Max(const std::vector<T>& source)
    {
        if (source.empty()) throw System::InvalidOperationException("Sequence contains no elements.");
        if constexpr (std::is_floating_point_v<T>) {
            std::size_t i = 0;
            while (i < source.size() && std::isnan(source[i])) ++i;
            if (i == source.size()) return source.back();
            T value = source[i];
            for (; i < source.size(); ++i)
                if (source[i] > value) value = source[i];
            return value;
        } else {
            return *std::max_element(source.begin(), source.end());
        }
    }

    /**
     * @brief Returns elements sorted ascending by @p keySelector.
     *
     * C++ counterpart of .NET Enumerable.OrderBy. Real .NET's OrderBy is explicitly documented
     * as a stable sort ("if the keys of two elements are equal, the order of the elements is
     * preserved") -- confirmed against the reference source's own internal machinery
     * (OrderBy.cs's ImplicitlyStableOrderedIterator exists specifically to preserve this
     * guarantee). std::sort gives no such guarantee (introsort-based implementations may
     * reorder equal-key elements arbitrarily); std::stable_sort is required to match.
     *
     * @throws System::ArgumentNullException if @p keySelector is an empty std::function.
     *         Checked before the sort, so a sequence of fewer than two elements -- which
     *         std::stable_sort never compares, and which therefore used to succeed with an
     *         empty key selector -- reports the argument error too. .NET checks eagerly in
     *         OrderedIterator's constructor (OrderedEnumerable.cs:80-82).
     */
    template<typename T, typename Key>
    std::vector<T> OrderBy(const std::vector<T>& source,
                           std::function<Key(const T&)> keySelector)
    {
        detail::requireCallable(keySelector, "keySelector");
        std::vector<T> result = source;
        const System::detail::DefaultLess<Key> less{};
        std::stable_sort(result.begin(), result.end(),
                  [&](const T& a, const T& b) { return less(keySelector(a), keySelector(b)); });
        return result;
    }

    /**
     * @brief Returns elements sorted descending by @p keySelector.
     *
     * C++ counterpart of .NET Enumerable.OrderByDescending. Same stable-sort requirement as
     * OrderBy above -- see its comment for the rationale.
     *
     * @throws System::ArgumentNullException if @p keySelector is an empty std::function,
     *         on the same terms as OrderBy above.
     */
    template<typename T, typename Key>
    std::vector<T> OrderByDescending(const std::vector<T>& source,
                                     std::function<Key(const T&)> keySelector)
    {
        detail::requireCallable(keySelector, "keySelector");
        std::vector<T> result = source;
        const System::detail::DefaultGreater<Key> greater{};
        std::stable_sort(result.begin(), result.end(),
                  [&](const T& a, const T& b) { return greater(keySelector(a), keySelector(b)); });
        return result;
    }

    /**
     * @brief Returns distinct elements, preserving the first occurrence.
     *
     * C++ counterpart of .NET Enumerable.Distinct, which uses
     * `EqualityComparer<T>.Default` — so for `float` and `double` two NaN elements
     * are duplicates of each other and only the first survives.
     */
    template<typename T>
    std::vector<T> Distinct(const std::vector<T>& source)
    {
        std::vector<T> result;
        for (const auto& item : source) {
            bool found = false;
            for (const auto& existing : result)
                if (System::detail::equalValues(existing, item)) { found = true; break; }
            if (!found) result.push_back(item);
        }
        return result;
    }

    /** @brief Returns the elements in reverse order. */
    template<typename T>
    std::vector<T> Reverse(const std::vector<T>& source)
    {
        std::vector<T> result(source.rbegin(), source.rend());
        return result;
    }

    /** @brief Skips the first @p count elements and returns the rest. */
    template<typename T>
    std::vector<T> Skip(const std::vector<T>& source, intcs count)
    {
        if (count <= 0) return source;
        if (static_cast<size_t>(count) >= source.size()) return {};
        return std::vector<T>(source.begin() + count, source.end());
    }

    /** @brief Returns the first @p count elements. */
    template<typename T>
    std::vector<T> Take(const std::vector<T>& source, intcs count)
    {
        if (count <= 0) return {};
        size_t n = std::min(static_cast<size_t>(count), source.size());
        return std::vector<T>(source.begin(), source.begin() + n);
    }

    /** @brief Concatenates two sequences. */
    template<typename T>
    std::vector<T> Concat(const std::vector<T>& first, const std::vector<T>& second)
    {
        std::vector<T> result = first;
        result.insert(result.end(), second.begin(), second.end());
        return result;
    }

    /**
     * @brief Returns true if @p source contains @p value.
     *
     * C++ counterpart of .NET Enumerable.Contains, which uses
     * `EqualityComparer<T>.Default` — so a NaN @p value is found in a sequence that
     * holds a NaN, which raw `==` never was.
     */
    template<typename T>
    bool Contains(const std::vector<T>& source, const T& value)
    {
        for (const auto& item : source)
            if (System::detail::equalValues(item, value)) return true;
        return false;
    }

} // namespace System::Linq
