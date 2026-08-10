// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/detail/ComparisonPolicy.hpp"

namespace System::Collections::Frozen {

/**
 * @brief Provides an immutable, read-only set optimized for fast lookup and enumeration.
 *
 * C++ counterpart of .NET System.Collections.Frozen.FrozenSet<T>.
 * Once created via Create() or ToFrozenSet(), the set cannot be modified.
 * Backed by std::unordered_set keyed under @c EqualityComparer<T>.Default for O(1)
 * average-case lookups.
 * Ideal for data loaded once at startup and read frequently at runtime.
 *
 * @par Default equality and hashing (ticket #1919)
 * A frozen projection must agree with the source it was frozen from, so this class uses the
 * same @ref SetType predicates `HashSet<T>` does — `System::detail::DefaultKeyHash<T>` and
 * `System::detail::DefaultKeyEqual<T>`, token-identical to the standard predicates outside
 * floating and direct nullable-floating forms. Before this ticket, freezing
 * `{NaN, NaN, 1}` produced a set of **Count 3** whose `Contains(NaN)` answered **false**:
 * two entries that are one element in .NET, neither of them reachable. Repairing the mutable
 * containers and leaving the projections alone would have made a frozen copy disagree with
 * its own source, which is worse than either alone.
 *
 * @warning **PUBLIC TYPE CHANGE for a floating-point or direct nullable-floating element**
 * (tickets #1919/#1925, approved). For `T` = `float`, `double`, `long double`, or direct
 * `std::optional` of one of them, @ref const_iterator and the parameter type of
 * @ref CreateFromSet are @ref SetType's, not `std::unordered_set<T>`'s. Spell @ref SetType to
 * be correct for every element type. All other instantiations are unaffected. See
 * docs/Migration-CollectionsFloatingComparers.md.
 *
 * @tparam T The type of the elements.
 */
template<typename T>
class FrozenSet {
public:
    /**
     * @brief The backing set type: a `std::unordered_set` keyed under
     *        @c EqualityComparer<T>.Default.
     *
     * Token-identical to `std::unordered_set<T>` for every unaffected @p T (tickets
     * #1919/#1925).
     */
    using SetType = std::unordered_set<T,
                                       System::detail::DefaultKeyHash<T>,
                                       System::detail::DefaultKeyEqual<T>>;

private:
    SetType set_;

    explicit FrozenSet(SetType s) : set_(std::move(s)) {}

public:
    /** @brief Const iterator over @ref SetType. See the class warning (ticket #1919). */
    using const_iterator = typename SetType::const_iterator;

    /**
     * @brief Gets an empty FrozenSet singleton.
     *
     * C++ counterpart of .NET FrozenSet<T>.Empty.
     */
    static const FrozenSet& getEmptyProperty() {
        static FrozenSet instance{SetType{}};
        return instance;
    }

    /**
     * @brief Creates a FrozenSet from a vector of elements.
     *
     * C++ counterpart of .NET FrozenSet.Create(ReadOnlySpan<T>).
     * Duplicate elements are silently ignored.
     * @param source Elements to populate the set.
     * @return A new immutable FrozenSet.
     */
    static FrozenSet Create(const std::vector<T>& source) {
        SetType s;
        s.reserve(source.size());
        for (const auto& item : source) s.insert(item);
        return FrozenSet(std::move(s));
    }

    /**
     * @brief Creates a FrozenSet from an existing unordered_set.
     *
     * @param source The set whose elements are copied into the frozen set. Its type is
     *               @ref SetType, which is the raw standard set for every unaffected @p T
     *               (tickets #1919/#1925).
     * @return A new immutable FrozenSet.
     */
    static FrozenSet CreateFromSet(const SetType& source) {
        return FrozenSet(SetType(source));
    }

    /**
     * @brief Gets the number of elements in the set.
     *
     * C++ counterpart of .NET FrozenSet<T>.Count.
     */
    [[nodiscard]] SharpRuntime::intcs getCountProperty() const {
        return static_cast<SharpRuntime::intcs>(set_.size());
    }

    /**
     * @brief Gets a vector of all items in the set.
     *
     * C++ counterpart of .NET FrozenSet<T>.Items.
     */
    [[nodiscard]] std::vector<T> getItemsProperty() const {
        return std::vector<T>(set_.begin(), set_.end());
    }

    /**
     * @brief Determines whether the set contains the specified element.
     *
     * C++ counterpart of .NET FrozenSet<T>.Contains(T).
     * @param item The element to locate.
     * @return true if found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const {
        return set_.count(item) != 0;
    }

    /**
     * @brief Attempts to get the actual stored value equal to the given value.
     *
     * C++ counterpart of .NET FrozenSet<T>.TryGetValue(T, out T).
     * Useful when the equality comparer distinguishes objects that compare equal.
     * @param equalValue  The value to search for.
     * @param actualValue Receives the stored element if found.
     * @return true if an equal element was found; otherwise false.
     */
    bool TryGetValue(const T& equalValue, T& actualValue) const {
        auto it = set_.find(equalValue);
        if (it == set_.end()) return false;
        actualValue = *it;
        return true;
    }

    /**
     * @brief Copies all elements to the destination vector starting at the given index.
     *
     * C++ counterpart of .NET FrozenSet<T>.CopyTo(T[], int).
     * @param destination Destination vector; must already have room for index + Count elements.
     * @param index       Zero-based index at which copying begins.
     * @throws System::ArgumentOutOfRangeException if @p index is negative.
     * @throws System::ArgumentException if @p destination is not large enough.
     */
    void CopyTo(std::vector<T>& destination, SharpRuntime::intcs index) const {
        if (index < 0) throw System::ArgumentOutOfRangeException("destinationIndex");
        if (static_cast<std::size_t>(index) + set_.size() > destination.size())
            throw System::ArgumentException("Destination too small for the number of elements in the collection.", "destination");
        std::size_t i = static_cast<std::size_t>(index);
        for (const auto& item : set_) {
            destination[i++] = item;
        }
    }

    /** @brief Returns an iterator to the beginning of the set (for range-based for). */
    [[nodiscard]] const_iterator begin() const { return set_.begin(); }

    /** @brief Returns an iterator past the end of the set (for range-based for). */
    [[nodiscard]] const_iterator end()   const { return set_.end();   }
};

/**
 * @brief Creates a FrozenSet from a vector of elements.
 *
 * C++ counterpart of .NET IEnumerable<T>.ToFrozenSet().
 * Duplicate elements are silently ignored.
 * @param source Elements to populate the set.
 * @return A new immutable FrozenSet.
 */
template<typename T>
FrozenSet<T> ToFrozenSet(const std::vector<T>& source) {
    return FrozenSet<T>::Create(source);
}

} // namespace System::Collections::Frozen
