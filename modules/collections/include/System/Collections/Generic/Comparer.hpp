// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Collections/Generic/IComparer.hpp"
#include "System/Collections/Generic/EqualityComparer.hpp"
#include "System/detail/ComparisonPolicy.hpp"

namespace System::Collections::Generic {

using SharpRuntime::intcs;

/**
 * @brief Provides a base class for implementations of IComparer<T>.
 *
 * C++ counterpart of .NET System.Collections.Generic.Comparer<T>.
 * Derive from this class and override Compare to implement a custom ordering.
 *
 * @tparam T The type of objects to compare.
 */
template<typename T>
class Comparer : public IComparer<T> {
public:
    /** @brief Virtual destructor for safe polymorphic destruction. */
    virtual ~Comparer() = default;

    /**
     * @brief When overridden in a derived class, compares two objects and returns an ordering integer.
     *
     * C++ counterpart of .NET Comparer<T>.Compare(T, T).
     * @param x The first object to compare.
     * @param y The second object to compare.
     * @return Negative if x < y, zero if x == y, positive if x > y.
     */
    [[nodiscard]] virtual intcs Compare(const T& x, const T& y) const override = 0;

    /**
     * @brief Gets the default comparer for type T.
     *
     * C++ counterpart of .NET Comparer<T>.Default, which is
     * `GenericComparer<T>.Compare` — `x.CompareTo(y)`, *not* the operand type's
     * `operator<`. For ordinary non-nullable types in this port the two agree
     * except for floating primitives, whose `Single.CompareTo`/`Double.CompareTo` order
     * NaN **before every value including negative infinity** and treat two NaNs
     * as equal. Routed through @c System::detail::compareValues so this class
     * and every collection that ports a `Comparer<T>.Default` expression state
     * the rule once. The same helper maps exactly the three direct
     * `std::optional<F>` floating forms to .NET's null-first Nullable comparer;
     * other optional and composite types keep their existing operator behavior.
     *
     * @note Using the built-in `<` here did not merely give a different answer:
     * `Compare(NaN, x)` returned `0` — *equivalent* — for every `x`, which makes
     * the induced equivalence intransitive and this object an invalid comparator
     * for `std::sort`, `std::lower_bound` and every ordered associative
     * container a caller might hand it to. See
     * `docs/CollectionsComparisonContractPlan.md` §2.1.
     *
     * @return A singleton comparer implementing .NET's default ordering for T.
     */
    static const Comparer<T>& Default() {
        static struct DefaultComparer : Comparer<T> {
            [[nodiscard]] intcs Compare(const T& x, const T& y) const override {
                return static_cast<intcs>(System::detail::compareValues(x, y));
            }
        } instance;
        return instance;
    }

    /**
     * @brief Creates a comparer from a comparison function.
     *
     * C++ counterpart of .NET Comparer<T>.Create(Comparison<T>).
     * @param comparison A function that compares two T values and returns an ordering integer.
     * @return A new heap-allocated Comparer wrapping the function (caller owns the pointer).
     */
    static Comparer<T>* Create(std::function<intcs(const T&, const T&)> comparison) {
        struct LambdaComparer : Comparer<T> {
            std::function<intcs(const T&, const T&)> fn_;
            explicit LambdaComparer(std::function<intcs(const T&, const T&)> f) : fn_(std::move(f)) {}
            [[nodiscard]] intcs Compare(const T& x, const T& y) const override { return fn_(x, y); }
        };
        return new LambdaComparer(std::move(comparison));
    }
};

} // namespace System::Collections::Generic
