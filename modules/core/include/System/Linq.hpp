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
#include "System/OverflowException.hpp"
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
     * DefaultIfEmpty, Chunk, and more -- add on demand as ported code needs them.
     *
     * @note **How this squares with CLAUDE.md rule 8, "No LINQ".** That rule governs the code
     * THIS project writes: ported bodies use `std::ranges`, not these operators. This header is
     * the other side of it -- a compatibility surface for ported C#/XNA call sites that already
     * spell `Where`/`Select`/`FirstOrDefault`.
     *
     * @note **Measured 2026-08-19, because the previous wording claimed more than is true.** It
     * said this "exists only to support already-ported C#/XNA call sites that use these
     * operators", which implies such sites exist. They do not, anywhere: a strict search for
     * `System::Linq::` finds **zero** uses in this repository's production code, **zero** in
     * `cna` and **zero** in `mobile-eggbert`. Its only users are its own two test files
     * (`LinqTests.cpp` and `ComparisonContractTests.cpp`).
     *
     * So this is a capability offered **in advance** of a caller rather than a response to one.
     * That is a deliberate position and not a defect -- the surface is what lets a future port
     * land without first re-litigating rule 8 -- but it should be read as such, and its removal
     * would be a public-surface decision rather than a cleanup.
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

        /**
         * @brief The accumulation step of both `Sum` overloads, with a checked domain for the
         *        signed integral types and every other domain left exactly as it was.
         *
         * `result = result + item` reached UBSan-confirmed signed overflow for `int`,
         * `long` and `long long` (SR-AUD-135, ticket #2220). Current .NET's
         * `Enumerable.Sum` accumulates in a **checked** context and raises
         * `OverflowException`, so this is a CCF-004 class C repair: detect the overflow
         * *before* it happens and throw. (CCF-004 itself stays closed 8/8 and gains no
         * member; this is an occurrence of its cause, recorded in
         * `docs/CoreDefinedArithmeticBoundedParseFamilyPlan.md`.)
         *
         * The domain is stated deliberately rather than swept, because measurement showed
         * the finding's framing was wider than the undefined behaviour:
         *
         *  - **signed integral, excluding `char` and `bool`** — checked, throws. The sum is
         *    formed in the unsigned counterpart, where wrap is defined, and the overflow is
         *    recognised from the operand signs, so nothing undefined is executed on the way
         *    to the diagnosis. .NET has `Sum` overloads for `int` and `long` and this
         *    matches them; the narrower fixed-width siblings follow for consistency.
         *  - **`char`** — excluded by name. Its signedness is implementation-defined, so
         *    throwing for it would make this port's observable behaviour differ between
         *    x86-64 and AArch64; and .NET's `char` is not a summable numeric type at all.
         *  - **unsigned integral** — unchanged. `Sum<unsigned>({UINT_MAX, 1})` wraps to `0`
         *    today, which is *defined* C++, and .NET has no unsigned `Sum` overload to
         *    match, so there is nothing to correct it to.
         *  - **floating point** — unchanged. .NET's `float`/`double` `Sum` is not checked
         *    either; `Sum<double>({DBL_MAX, DBL_MAX})` stays `inf`.
         *  - **any other `T`** — unchanged `a + b`; the operator's own contract governs.
         *
         * Note that `short` and `signed char` were *never* undefined here (both promote to
         * `int` before the addition, so only the narrowing store was lossy) but are checked
         * all the same, because a silently truncated total is a wrong answer either way.
         *
         * @param a The running total.
         * @param b The next term.
         * @return The sum.
         * @throws System::OverflowException if a checked domain's sum is not representable.
         */
        template<typename T>
        [[nodiscard]] inline T addForSum(const T& a, const T& b) {
            if constexpr (std::is_integral_v<T> && std::is_signed_v<T> &&
                          !std::is_same_v<std::remove_cv_t<T>, char> &&
                          !std::is_same_v<std::remove_cv_t<T>, bool>) {
                using U = std::make_unsigned_t<T>;
                const T sum = static_cast<T>(static_cast<U>(static_cast<U>(a) + static_cast<U>(b)));
                // Two operands of the same sign whose sum has the other sign is exactly the
                // overflow condition, and it is evaluated on a value the wrap already produced
                // rather than on an operation the language leaves undefined.
                if ((a < 0) == (b < 0) && (sum < 0) != (a < 0))
                    throw System::OverflowException();
                return sum;
            } else {
                return a + b;
            }
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

    /**
     * @brief Returns the sum of all elements (requires operator+).
     *
     * C++ counterpart of .NET `Enumerable.Sum`. For the signed integral types the
     * accumulation is **checked**, exactly as the reference's is, and raises
     * `System::OverflowException` on the first term that leaves the type's range —
     * including a chain whose intermediate overflows even though the conceptual total
     * would be representable, which is what .NET's per-element check does too. Every
     * other element type keeps the behaviour it had; `detail::addForSum` states the whole
     * domain and its reasons (SR-AUD-135, ticket #2220).
     *
     * @throws System::OverflowException if a signed integral sum is not representable.
     */
    template<typename T>
    T Sum(const std::vector<T>& source)
    {
        T result{};
        for (const auto& item : source) result = detail::addForSum<T>(result, item);
        return result;
    }

    /**
     * @brief Returns the sum of a projected value (requires operator+).
     *
     * The projected accumulation is checked on exactly the same domain as the plain
     * overload above; the selector's own result type decides.
     *
     * @throws System::ArgumentNullException if @p selector is an empty std::function.
     * @throws System::OverflowException if a signed integral sum is not representable.
     */
    template<typename T, typename R>
    R Sum(const std::vector<T>& source, std::function<R(const T&)> selector)
    {
        detail::requireCallable(selector, "selector");
        R result{};
        for (const auto& item : source) result = detail::addForSum<R>(result, selector(item));
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
