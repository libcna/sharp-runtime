// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <stdexcept>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Collections/Generic/IList.hpp"
#include "System/Collections/detail/ElementReference.hpp"
#include "System/Collections/detail/MutationCounter.hpp"
#include "System/detail/ComparisonPolicy.hpp"

namespace System::Collections::ObjectModel {

using SharpRuntime::intcs;

/**
 * @brief Provides the base class for a generic collection.
 *
 * C++ counterpart of .NET System.Collections.ObjectModel.Collection<T>.
 * Backed by std::vector<T>; exposes virtual hook methods (InsertItem, RemoveItem,
 * ClearItems, SetItem) that derived classes can override to intercept changes.
 *
 * @note **Ticket #1791 gave this type a mutation counter**, which it previously did not
 * have, so that its tracked indexer can invalidate an outstanding enumerator on an indexed
 * write. Two consequences, both measured and recorded in
 * `docs/ListIndexerVersioningDesign.md`:
 *  - `sizeof(Collection<T>)` grew from 32 to 40 bytes on LP64, so every consumer must be
 *    rebuilt.
 *  - `GetEnumerator()`'s enumerator is now **fail-fast**. Before #1791 it version-checked
 *    nothing at all -- not even `Add()` -- because there was no counter to check, so
 *    mutating during enumeration silently read reallocated storage. It now throws
 *    `System::InvalidOperationException` exactly as `Generic::List<T>`'s does, which is also
 *    what .NET does: .NET's `Collection<T>.GetEnumerator()` delegates to the wrapped
 *    `IList<T>`'s enumerator, normally `List<T>`'s fail-fast one.
 *
 * @tparam T The type of elements in the collection.
 */
template<typename T>
class Collection : public Generic::IList<T> {
private:
    class Enumerator : public Generic::IEnumerator<T> {
        const Collection<T>* owner_;
        System::Collections::detail::MutationVersion version_;
        intcs index_ = -1;
        System::Collections::detail::EnumeratorState state_;
    public:
        explicit Enumerator(const Collection<T>* owner) : owner_(owner), version_(owner->version_) {}
        bool MoveNext() override {
            System::Collections::detail::requireUnmodified(version_ == owner_->version_);
            if (state_.isAfterLast()) return false;

            const intcs next = index_ + 1;
            if (next < static_cast<intcs>(owner_->items_.size())) {
                index_ = next;
                state_.setCurrent();
                return true;
            }

            index_ = static_cast<intcs>(owner_->items_.size());
            state_.setAfterLast();
            return false;
        }
        void Reset() override {
            System::Collections::detail::requireUnmodified(version_ == owner_->version_);
            index_ = -1;
            state_.Reset();
        }
        [[nodiscard]] const T& Current() const override {
            state_.requireCurrent();
            return owner_->items_[static_cast<size_t>(index_)];
        }
    };

    void requireIndexInRange(intcs index) const {
        if (index < 0 || index >= static_cast<intcs>(items_.size()))
            throw System::ArgumentOutOfRangeException("index", "Index was out of range. Must be non-negative and less than the size of the collection.");
    }

    /**
     * Counts effective mutations so an outstanding enumerator can fail fast, and so the
     * tracked indexer has something to advance. Added by ticket #1791; see the class note.
     */
    System::Collections::detail::MutationCounter version_;

    /**
     * Test-only seam (declared in detail/MutationCounter.hpp, never defined in production,
     * defined for every collection in exactly one test header -- ticket #1800) letting a
     * regression observe the counter or position it near a boundary.
     */
    friend struct SharpRuntime::Testing::CollectionVersionAccess<Collection<T>>;

protected:
    /** @brief The underlying storage for collection items. */
    std::vector<T> items_;

    /**
     * @brief Inserts @p item at @p index into items_; override to intercept insertion.
     *
     * C++ counterpart of .NET Collection<T>.InsertItem(int, T).
     * @param index The zero-based index at which to insert.
     * @param item  The element to insert.
     * @throws System::ArgumentOutOfRangeException if @p index is not within [0, Count].
     */
    virtual void InsertItem(intcs index, const T& item) {
        if (index < 0 || index > static_cast<intcs>(items_.size()))
            throw System::ArgumentOutOfRangeException("index", "Index must be within the bounds of the List.");
        items_.insert(items_.begin() + index, item);
        ++version_;
    }

    /**
     * @brief Removes the item at @p index from items_; override to intercept removal.
     *
     * C++ counterpart of .NET Collection<T>.RemoveItem(int).
     * @param index The zero-based index of the element to remove.
     * @throws System::ArgumentOutOfRangeException if @p index is out of range.
     */
    virtual void RemoveItem(intcs index) {
        requireIndexInRange(index);
        items_.erase(items_.begin() + index);
        ++version_;
    }

    /**
     * @brief Removes all items from items_; override to intercept clear.
     *
     * C++ counterpart of .NET Collection<T>.ClearItems().
     */
    virtual void ClearItems() {
        items_.clear();
        ++version_;
    }

    /**
     * @brief Replaces the item at @p index; override to intercept replacement.
     *
     * C++ counterpart of .NET Collection<T>.SetItem(int, T).
     * @param index The zero-based index of the element to replace.
     * @param item  The new value.
     * @throws System::ArgumentOutOfRangeException if @p index is out of range.
     */
    virtual void SetItem(intcs index, const T& item) {
        requireIndexInRange(index);
        System::Collections::detail::ElementReference<T>{
            &items_[static_cast<size_t>(index)], &version_} = item;
    }

public:
    /** @brief Default-constructs an empty Collection. */
    Collection() = default;
    /** @brief Virtual destructor for safe polymorphic destruction. */
    ~Collection() override = default;

    /**
     * @brief Gets the number of elements in the collection.
     *
     * C++ counterpart of .NET Collection<T>.Count.
     * @return The number of elements.
     */
    [[nodiscard]] intcs getCountProperty() const override {
        return static_cast<intcs>(items_.size());
    }

    /**
     * @brief Gets a value indicating whether the collection is read-only.
     *
     * C++ counterpart of .NET ICollection<T>.IsReadOnly.
     * @return Always false — Collection<T> allows modifications.
     */
    [[nodiscard]] bool getIsReadOnlyProperty() const override { return false; }

    /**
     * @brief Adds @p item to the end of the collection.
     *
     * C++ counterpart of .NET Collection<T>.Add(T).
     * @param item The element to add.
     */
    void Add(const T& item) override {
        InsertItem(static_cast<intcs>(items_.size()), item);
    }

    /**
     * @brief Removes all elements from the collection.
     *
     * C++ counterpart of .NET Collection<T>.Clear().
     */
    void Clear() override { ClearItems(); }

    /**
     * @brief Determines whether the collection contains the specified element.
     *
     * C++ counterpart of .NET Collection<T>.Contains(T).
     * @param item The element to locate.
     * @return true if found; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const override {
        return System::detail::findValue(items_.begin(), items_.end(), item)
               != items_.end();
    }

    /**
     * @brief Copies all elements to @p destination starting at @p index.
     *
     * C++ counterpart of .NET Collection<T>.CopyTo(T[], int).
     * @param destination The target vector to copy elements into.
     * @param index       The zero-based starting index in @p destination.
     */
    void CopyTo(std::vector<T>& destination, intcs index) const {
        if (index < 0)
            throw System::ArgumentOutOfRangeException("index", "Non-negative number required.");
        // Verified against List<T>.CopyTo's own bounds check (`array.Length - arrayIndex <
        // Count`): a subtraction-based check, not addition. `index + getCountProperty() >
        // destination.size()` is signed-integer-overflow UB for a large index (confirmed via
        // UBSan) -- and worse than UB in the abstract: the wraparound can make the check
        // evaluate false when it should be true, silently skipping the bounds throw and letting
        // the loop below write out of bounds into destination. index is already known
        // non-negative here, and destination.size() cast to intcs is likewise non-negative, so
        // the subtraction cannot itself overflow.
        if (static_cast<intcs>(destination.size()) - index < getCountProperty())
            throw System::ArgumentException("Destination array is not long enough to copy all the items in the collection.");
        for (intcs i = 0; i < getCountProperty(); ++i)
            destination[static_cast<size_t>(index + i)] = items_[static_cast<size_t>(i)];
    }

    /**
     * @brief Removes the first occurrence of @p item from the collection.
     *
     * C++ counterpart of .NET Collection<T>.Remove(T).
     * @param item The element to remove.
     * @return true if the element was found and removed; otherwise false.
     */
    bool Remove(const T& item) override {
        auto it = System::detail::findValue(items_.begin(), items_.end(), item);
        if (it == items_.end()) return false;
        RemoveItem(static_cast<intcs>(it - items_.begin()));
        return true;
    }

    /**
     * @brief Returns a const reference to the element at the specified index.
     *
     * C++ counterpart of .NET Collection<T>.Item[int] getter.
     * @param index The zero-based index.
     * @return A const reference to the element.
     * @throws System::ArgumentOutOfRangeException if index is out of range.
     */
    [[nodiscard]] const T& operator[](intcs index) const override {
        requireIndexInRange(index);
        return items_[static_cast<size_t>(index)];
    }

    /**
     * @brief Returns a tracked reference proxy for the element at the specified index.
     *
     * C++ counterpart of .NET Collection<T>.Item[int] setter. Since ticket #1791 a write
     * through the returned proxy advances this collection's mutation counter, so an
     * outstanding enumerator fails fast.
     *
     * @note **The `SetItem` hook is still not run by this operator.** .NET's indexer setter
     *       dispatches through the virtual `SetItem(index, value)` hook, so a derived class
     *       overriding it (to keep an auxiliary index in sync, as
     *       `KeyedCollection<TKey,TItem>` does) sees every assignment. The tracked proxy
     *       intercepts the *write* -- which is what mutation tracking needs -- but it holds a
     *       slot and a counter, not a collection, so it cannot make a virtual call. Ticket
     *       #1791 narrowed this gap rather than closing it: the assignment is now tracked,
     *       where before it was both untracked and hook-skipping. **Use `setItem(index,
     *       value)`, which does dispatch through the `SetItem` hook**, whenever a
     *       `SetItem`-overriding derived class must see the assignment.
     * @param index The zero-based index.
     * @return A tracked reference proxy for the element.
     * @throws System::ArgumentOutOfRangeException if index is out of range.
     */
    System::Collections::detail::ElementReference<T> operator[](intcs index) override {
        requireIndexInRange(index);
        return {&items_[static_cast<size_t>(index)], &version_};
    }

    /**
     * @brief Gets the element at the specified index without mutating anything.
     *
     * C++ counterpart of .NET Collection<T>.Item[int]'s getter. Advances no mutation counter.
     * @param index The zero-based index.
     * @return A const reference to the element.
     * @throws System::ArgumentOutOfRangeException if index is out of range.
     */
    [[nodiscard]] const T& getItem(intcs index) const override {
        requireIndexInRange(index);
        return items_[static_cast<size_t>(index)];
    }

    /**
     * @brief Replaces the element at the specified index, through the `SetItem` hook.
     *
     * C++ counterpart of .NET Collection<T>.Item[int]'s setter, including its dispatch
     * through the virtual `SetItem(index, value)` hook -- so a derived class that overrides
     * `SetItem` sees this assignment, which is exactly what the plain indexer cannot offer.
     * The default `SetItem` validates the index before writing and advances the mutation
     * counter exactly once.
     *
     * @param index The zero-based index of the element to replace.
     * @param value The new value.
     * @throws System::ArgumentOutOfRangeException if index is out of range.
     */
    void setItem(intcs index, const T& value) override { SetItem(index, value); }

    /**
     * @brief Returns the zero-based index of the first occurrence of @p item, or -1 if not found.
     *
     * C++ counterpart of .NET Collection<T>.IndexOf(T).
     * @param item The element to locate.
     * @return The zero-based index, or -1 if not found.
     */
    [[nodiscard]] intcs IndexOf(const T& item) const override {
        auto it = System::detail::findValue(items_.begin(), items_.end(), item);
        if (it == items_.end()) return -1;
        return static_cast<intcs>(it - items_.begin());
    }

    /**
     * @brief Inserts @p item at the specified index.
     *
     * C++ counterpart of .NET Collection<T>.Insert(int, T).
     * @param index The zero-based index at which to insert.
     * @param item  The element to insert.
     */
    void Insert(intcs index, const T& item) override { InsertItem(index, item); }

    /**
     * @brief Removes the element at the specified index.
     *
     * C++ counterpart of .NET Collection<T>.RemoveAt(int).
     * @param index The zero-based index of the element to remove.
     */
    void RemoveAt(intcs index) override { RemoveItem(index); }

    /**
     * @brief Returns a new enumerator that iterates through the collection.
     *
     * C++ counterpart of .NET Collection<T>.GetEnumerator().
     * @return A heap-allocated IEnumerator<T>; caller takes ownership.
     */
    Generic::IEnumerator<T>* GetEnumerator() override {
        return new Enumerator(this);
    }

    /**
     * @brief Returns an iterator to the beginning of the collection (STL interop).
     *
     * @warning Dereferencing yields a mutable `T&`; a write through it advances no mutation
     * counter and no outstanding enumerator will see it. Prefer the tracked indexer or
     * `setItem`.
     */
    auto begin()       { return items_.begin(); }
    /** @brief Returns an iterator past the end of the collection (STL interop). */
    auto end()         { return items_.end(); }
    /** @brief Returns a const iterator to the beginning of the collection (STL interop). */
    [[nodiscard]] auto begin() const { return items_.cbegin(); }
    /** @brief Returns a const iterator past the end of the collection (STL interop). */
    [[nodiscard]] auto end()   const { return items_.cend(); }
};

} // namespace System::Collections::ObjectModel
