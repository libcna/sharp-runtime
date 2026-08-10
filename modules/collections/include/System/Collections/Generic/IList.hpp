// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Collections/Generic/ICollection.hpp"
#include "System/Collections/detail/ElementReference.hpp"

namespace System::Collections::Generic {

using SharpRuntime::intcs;

/**
 * @brief Represents a generic collection of objects that can be individually
 *        accessed by index.
 *
 * C++ counterpart of .NET System.Collections.Generic.IList<T>.
 * Extends ICollection<T> with index-based access, insertion, and removal.
 *
 * @note The **non-const** indexer returns a tracked
 *       `System::Collections::detail::ElementReference<T>` proxy rather than a plain `T&`,
 *       so that an implementer can advance its mutation counter on an indexed write exactly
 *       as .NET's `List<T>` index setter advances `_version`. A plain `T&` offers no such
 *       interception point. Ticket #1791 made this change; the design record, the measured
 *       alternatives, and the migration table are in `docs/ListIndexerVersioningDesign.md`.
 *
 * @warning **Implementing this interface by hand changed with ticket #1791.** The non-const
 *          `operator[]` must return `ElementReference<T>`, and `getItem`/`setItem` must be
 *          implemented. An implementer that can mutate needs a
 *          `System::Collections::detail::MutationCounter` to hand the proxy; a read-only
 *          implementer should throw `System::NotSupportedException` from both the non-const
 *          indexer and `setItem`, as `ObjectModel::ReadOnlyCollection<T>` does.
 *
 * @tparam T The type of elements in the list.
 */
template<typename T>
class IList : public ICollection<T> {
public:
    /** @brief Virtual destructor for safe polymorphic destruction. */
    ~IList() override = default;

    /**
     * @brief Gets the element at the specified index (const).
     *
     * C++ counterpart of .NET IList<T>.Item[int] getter.
     * @param index The zero-based index of the element to get.
     * @return A const reference to the element at the specified index.
     */
    [[nodiscard]] virtual const T& operator[](intcs index) const = 0;

    /**
     * @brief Gets or sets the element at the specified index (mutable, tracked).
     *
     * C++ counterpart of .NET IList<T>.Item[int] setter. Returns a tracked proxy: reading
     * through it advances nothing, and every write through it (`list[i] = v`, a compound
     * assignment, `++`/`--`) advances the implementer's mutation counter exactly once, so an
     * outstanding fail-fast enumerator is invalidated exactly as .NET's is.
     *
     * @param index The zero-based index of the element to get or set.
     * @return A tracked reference proxy for the element at the specified index.
     */
    virtual System::Collections::detail::ElementReference<T> operator[](intcs index) = 0;

    /**
     * @brief Gets the element at the specified index.
     *
     * C++ counterpart of .NET IList<T>.Item[int] getter, spelled as an explicit accessor.
     * Performs no mutation and advances no mutation counter.
     *
     * @param index The zero-based index of the element to get.
     * @return A const reference to the element, following `std::vector` invalidation rules.
     */
    [[nodiscard]] virtual const T& getItem(intcs index) const = 0;

    /**
     * @brief Replaces the element at the specified index (tracked).
     *
     * C++ counterpart of .NET IList<T>.Item[int] setter, spelled as an explicit accessor and
     * matching the `getItem`/`setItem` convention this codebase already uses for indexers.
     * Validates @p index before writing anything, and advances the implementer's mutation
     * counter exactly once on an effective replacement -- including when the new value equals
     * the old one, which is what .NET does (`List.cs:161-162` never compares).
     *
     * @param index The zero-based index of the element to replace.
     * @param value The new value.
     */
    virtual void setItem(intcs index, const T& value) = 0;

    /**
     * @brief Determines the index of a specific item in the list.
     *
     * C++ counterpart of .NET IList<T>.IndexOf(T).
     * @param item The object to locate in the list.
     * @return The index of the item if found; otherwise -1.
     */
    [[nodiscard]] virtual intcs IndexOf(const T& item) const = 0;

    /**
     * @brief Inserts an item at the specified index.
     *
     * C++ counterpart of .NET IList<T>.Insert(int, T).
     * @param index The zero-based index at which to insert the item.
     * @param item  The object to insert.
     */
    virtual void Insert(intcs index, const T& item) = 0;

    /**
     * @brief Removes the element at the specified index.
     *
     * C++ counterpart of .NET IList<T>.RemoveAt(int).
     * @param index The zero-based index of the element to remove.
     */
    virtual void RemoveAt(intcs index) = 0;
};

} // namespace System::Collections::Generic
