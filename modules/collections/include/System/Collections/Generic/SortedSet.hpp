// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <atomic>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/InvalidOperationException.hpp"

namespace System::Collections::Generic {

using SharpRuntime::intcs;

/**
 * @brief Represents a collection of objects maintained in sorted order with no duplicate elements.
 *
 * C++ counterpart of .NET System.Collections.Generic.SortedSet<T>.
 * Backed by std::set<T>; provides O(log n) Add, Remove, and Contains.
 *
 * @par Two roles, one type
 * A SortedSet<T> object is either an **owning full set** (created by a public constructor,
 * with no active bounds) or a **bounded live view** (created by GetViewBetween, with an
 * inclusive lower and upper bound). Both roles are the same public type, exactly as .NET's
 * `TreeSubSet` *is a* `SortedSet<T>`. getIsViewProperty() distinguishes them.
 *
 * The elements are owned by reference-counted state shared between an owning set and every
 * live view derived from it, so a view is a genuine bidirectional write-through window onto
 * the same tree, matching .NET's `TreeSubSet` (ticket #1783, SR-AUD-361). A view keeps that
 * state alive, so a view -- or an iterator -- that outlives the object it was obtained from
 * stays valid and well-defined; this is the C++ expression of .NET's strong `_underlying`
 * reference, which the garbage collector honours the same way.
 *
 * @par Copy, move, and assignment
 * Copying **preserves the object's role**: copying an owning set deep-clones its elements
 * into independent state (ordinary value semantics, unchanged from earlier revisions);
 * copying a view yields another handle onto the same state and bounds. Assignment
 * **rebinds** the assigned handle and never mutates state that another handle observes;
 * views and iterators onto the previous state keep that state alive and continue to observe
 * it. Move leaves the source a valid, empty owning set. Use ToSortedSet() to materialize an
 * independent, unbounded copy of any object's current elements.
 *
 * begin()/end() (used by range-for) detect structural modification during iteration
 * via a version counter and throw System::InvalidOperationException, matching real
 * .NET's fail-fast enumerator contract (see ticket 1713). There is exactly **one** version
 * counter per shared state, so a mutation made through an owning set invalidates the
 * enumerators of every view over it and vice versa -- again matching .NET, whose enumerator
 * compares against the single `_underlying.version`.
 *
 * @par Thread safety
 * No synchronization is provided, matching .NET (`ICollection.IsSynchronized => false`).
 * The contract has exactly two halves, and they are deliberately not the same:
 *
 * - **Concurrent mutation is unsupported.** If any thread mutates, no other thread may touch
 *   the collection at all. An owning set and every view derived from it are **one collection**
 *   for this purpose, so mutating through a view while reading through the parent (or through
 *   another view) is exactly as undefined as mutating a single set while reading it. This
 *   class adds no locking to make that safe, and none is planned.
 * - **Concurrent read-only access is race-free.** No `const` member of this class writes to an
 *   unsynchronized field, so any number of threads may call getCountProperty(), Contains(),
 *   getMinProperty(), getMaxProperty(), ToVector(), the read-only set predicates, or iterate,
 *   concurrently -- on the *same* object or on distinct handles over the same shared state --
 *   for as long as nobody mutates. This is the guarantee .NET documents for its own
 *   collections ("can support multiple readers concurrently, as long as the collection is not
 *   modified"), and it is the reason the lazy Count cache below is atomic rather than plain.
 *
 * Reference-count updates on the shared state are atomic, so copying or destroying handles on
 * different threads cannot corrupt the control block. Nothing beyond the two points above is
 * claimed: this type is not thread-safe, it is merely free of *internal* races when read.
 *
 * @note Ticket #1783 briefly broke the read-only half. It introduced a per-view lazy Count
 * cache held in two plain `mutable intcs` fields that the `const` getCountProperty() wrote, so
 * two threads reading Count through one view object executed conflicting non-atomic accesses
 * -- a genuine C++ data race, reported by ThreadSanitizer as "Read of size 4 ... Previous
 * write of size 4 ... in getCountProperty() const". Ticket #1784 repaired that by making the
 * two cache fields atomic with a release/acquire publication protocol. The cache, its cost,
 * and the object layout are otherwise unchanged.
 *
 * @tparam T The type of elements in the set. Only the ordering used by std::set<T> -- by
 *           default `std::less<T>`, hence `operator<` -- is required; no member of this class
 *           spells a comparison with `operator>`, `operator<=`, `operator>=`, or
 *           `operator==`. Range membership and set equality are decided by comparer
 *           equivalence (`!cmp(a,b) && !cmp(b,a)`).
 */
template<typename T>
class SortedSet {
    /**
     * @brief Independently owned, reference-counted tree state shared by an owning set and
     *        every live view derived from it.
     *
     * This is the C++ stand-in for the managed heap object .NET's `TreeSubSet` keeps alive
     * through its strong `_underlying` field. It owns the elements and the single version
     * counter that drives fail-fast enumeration for every handle onto it.
     */
    struct State {
        std::set<T> data;
        intcs version = 0;
    };

    std::shared_ptr<State> state_;
    // Absent == bound not active, mirroring .NET's _lBoundActive/_uBoundActive. An owning
    // full set has neither bound; GetViewBetween always activates both.
    std::optional<T> lower_;
    std::optional<T> upper_;
    // Version value meaning "this view has never computed its Count". The shared version
    // counter starts at 0 and only increments, so it never legitimately holds this value.
    static constexpr intcs kCountNotCached = -1;

    // .NET TreeSubSet's lazy `count`/`_countVersion` pair: a view's Count is recomputed only
    // once per version of the shared state.
    //
    // Atomic, not plain, because getCountProperty() is `const` yet fills them: two threads
    // reading Count through the same view object would otherwise perform conflicting
    // non-atomic accesses and make an observationally read-only operation undefined
    // behaviour (ticket #1784). getCountProperty() documents the publication protocol these
    // two fields obey; do not weaken their memory orders independently of each other.
    mutable std::atomic<intcs> cachedCount_{0};
    mutable std::atomic<intcs> cachedCountVersion_{kCountNotCached};

    // Ticket #1784 chose same-width atomics specifically so ticket #1783's published object
    // layout survives byte for byte -- sizeof(SortedSet<int>) == 40,
    // sizeof(SortedSet<std::string>) == 104. Fail loudly rather than silently re-breaking the
    // layout if a platform ever pads its atomics.
    static_assert(sizeof(std::atomic<intcs>) == sizeof(intcs),
                  "the atomic Count cache must not change SortedSet<T>'s object layout");
    static_assert(alignof(std::atomic<intcs>) == alignof(intcs),
                  "the atomic Count cache must not change SortedSet<T>'s alignment");

    using SetIterator = typename std::set<T>::const_iterator;

    // std::set::key_comp() returns BY VALUE, so binding it to a const reference would return
    // a reference to a temporary (-Wreturn-local-addr). Copy the predicate; never alias it.
    [[nodiscard]] typename std::set<T>::key_compare comparer() const {
        return state_->data.key_comp();
    }

    /** @brief First element of this object's range: lower_bound(*lower_) or begin(). */
    [[nodiscard]] SetIterator rangeBegin() const {
        return lower_.has_value() ? state_->data.lower_bound(*lower_) : state_->data.begin();
    }

    /** @brief One past the last element of this object's range: upper_bound(*upper_) or end(). */
    [[nodiscard]] SetIterator rangeEnd() const {
        return upper_.has_value() ? state_->data.upper_bound(*upper_) : state_->data.end();
    }

    /**
     * @brief Decides bound equality through comparer equivalence rather than operator==,
     *        so an element type providing only the documented ordering contract still works.
     */
    [[nodiscard]] bool sameBoundsAs(const SortedSet<T>& other) const {
        const auto cmp = comparer();
        const auto equivalent = [&cmp](const std::optional<T>& a, const std::optional<T>& b) {
            if (a.has_value() != b.has_value()) return false;
            if (!a.has_value()) return true;
            return !cmp(*a, *b) && !cmp(*b, *a);
        };
        return equivalent(lower_, other.lower_) && equivalent(upper_, other.upper_);
    }

    /** @brief True when @p other is a handle onto the same state with the same bounds. */
    [[nodiscard]] bool aliasesSameRangeAs(const SortedSet<T>& other) const {
        return other.state_ == state_ && sameBoundsAs(other);
    }

    /**
     * @brief Marks this handle's lazy Count cache as holding nothing.
     *
     * Called by every operation that rebinds the handle -- move construction, copy assignment,
     * move assignment -- so an assigned-to or moved-from object can never answer Count from a
     * value computed for state it no longer refers to. The stores follow getCountProperty()'s
     * protocol: the version is the publishing field, so it is written last, with `release`.
     */
    void invalidateCountCache() noexcept {
        cachedCount_.store(0, std::memory_order_relaxed);
        cachedCountVersion_.store(kCountNotCached, std::memory_order_release);
    }

    /** @brief Private view constructor: a bounded handle onto already-owned shared state. */
    SortedSet(std::shared_ptr<State> state, std::optional<T> lower, std::optional<T> upper)
        : state_(std::move(state)), lower_(std::move(lower)), upper_(std::move(upper)) {}

public:
    /**
     * @brief A version-checked wrapper over std::set<T>::const_iterator that throws
     * System::InvalidOperationException on dereference/increment if the shared state was
     * structurally modified since this iterator was obtained -- matching real .NET's fail-fast
     * enumerator contract (ticket 1713).
     *
     * The iterator holds its own strong reference to the enumerated state, so it can outlive
     * the SortedSet object it came from without reading freed memory, and assignment to that
     * object detaches this iterator rather than dangling it (ticket #1783).
     */
    class Iterator {
        std::shared_ptr<const State> state_;
        SetIterator it_;
        SetIterator end_;
        intcs version_ = 0;

        void checkVersion() const {
            if (version_ != state_->version)
                throw System::InvalidOperationException("Collection was modified; enumeration operation may not execute.");
        }

    public:
        /**
         * @brief Constructs an iterator over @p state positioned at @p it and bounded by @p end.
         * @param state The shared state being enumerated; kept alive by this iterator.
         * @param it    The current position.
         * @param end   One past the last position this iterator may reach.
         */
        Iterator(std::shared_ptr<const State> state, SetIterator it, SetIterator end)
            : state_(std::move(state)), it_(it), end_(end), version_(state_->version) {}

        /** @brief Returns the current element. */
        const T& operator*() const { checkVersion(); return *it_; }
        /** @brief Returns a pointer to the current element. */
        const T* operator->() const { checkVersion(); return &*it_; }
        /** @brief Advances to the next element in range, stopping at the range end. */
        Iterator& operator++() { checkVersion(); if (it_ != end_) ++it_; return *this; }
        /** @brief Compares positions; the version guard deliberately does not apply. */
        bool operator==(const Iterator& other) const { return it_ == other.it_; }
        /** @brief Compares positions; the version guard deliberately does not apply. */
        bool operator!=(const Iterator& other) const { return it_ != other.it_; }
    };

    /** @brief Initializes a new empty SortedSet that owns its own state. */
    SortedSet() : state_(std::make_shared<State>()) {}

    /**
     * @brief Initializes a SortedSet with elements from an initializer list.
     * @param items The initial elements.
     */
    explicit SortedSet(std::initializer_list<T> items) : state_(std::make_shared<State>()) {
        state_->data = std::set<T>(items);
    }

    /**
     * @brief Copy constructor. Copying preserves the object's role.
     *
     * An owning full set is deep-cloned into independent state, so the copy and the original
     * never observe each other's mutations. A bounded view is copied as another handle onto
     * the same shared state with the same bounds, so both handles stay live views of one
     * underlying set.
     * @param other The object to copy.
     */
    SortedSet(const SortedSet<T>& other) : lower_(other.lower_), upper_(other.upper_) {
        if (other.getIsViewProperty()) state_ = other.state_;
        else state_ = std::make_shared<State>(*other.state_);
    }

    /**
     * @brief Move constructor. Transfers the state handle and bounds.
     *
     * The moved-from object is left a valid, empty **owning** set -- never a view and never a
     * null handle -- so every member remains callable on it. Views and iterators onto the
     * transferred state are undisturbed and simply observe mutations made through the
     * destination object from now on.
     * @param other The object to move from.
     */
    SortedSet(SortedSet<T>&& other) noexcept
        : state_(std::move(other.state_)),
          lower_(std::move(other.lower_)),
          upper_(std::move(other.upper_)) {
        other.state_ = std::make_shared<State>();
        other.lower_.reset();
        other.upper_.reset();
        other.invalidateCountCache();
    }

    /**
     * @brief Copy assignment. Rebinds this handle; it never mutates state another handle observes.
     *
     * Assignment is equivalent to destroying this handle and copy-constructing it from
     * @p other, so views and iterators onto the previous state keep that state alive and
     * continue to observe its pre-assignment elements rather than silently switching to the
     * new contents.
     * @param other The object to copy.
     * @return A reference to this object.
     */
    SortedSet<T>& operator=(const SortedSet<T>& other) {
        if (this == &other) return *this;
        SortedSet<T> tmp(other);
        state_ = std::move(tmp.state_);
        lower_ = std::move(tmp.lower_);
        upper_ = std::move(tmp.upper_);
        invalidateCountCache();
        return *this;
    }

    /**
     * @brief Move assignment. Rebinds this handle and leaves the source a valid, empty owning set.
     * @param other The object to move from.
     * @return A reference to this object.
     */
    SortedSet<T>& operator=(SortedSet<T>&& other) noexcept {
        if (this == &other) return *this;
        state_ = std::move(other.state_);
        lower_ = std::move(other.lower_);
        upper_ = std::move(other.upper_);
        invalidateCountCache();
        other.state_ = std::make_shared<State>();
        other.lower_.reset();
        other.upper_.reset();
        other.invalidateCountCache();
        return *this;
    }

    /** @brief Releases this handle's reference to the shared state. */
    ~SortedSet() = default;

    /**
     * @brief Gets a value indicating whether this object is a bounded live view rather than an
     *        owning full set.
     *
     * There is no .NET counterpart property; in .NET the distinction is the runtime type
     * `TreeSubSet`. Provided so callers and tests can tell the two roles of this one type apart.
     * @return true if at least one bound is active; otherwise false.
     */
    [[nodiscard]] bool getIsViewProperty() const {
        return lower_.has_value() || upper_.has_value();
    }

    /**
     * @brief Determines whether @p item falls inside this object's bounds.
     *
     * C++ counterpart of .NET TreeSubSet.IsWithinRange(T). Both bounds are **inclusive**, and
     * the decision uses the underlying std::set's own ordering predicate. An owning full set
     * has no active bound, so every value is in range.
     * @param item The value to test.
     * @return true if @p item is within the active bounds; otherwise false.
     */
    [[nodiscard]] bool IsWithinRange(const T& item) const {
        const auto cmp = comparer();
        if (lower_.has_value() && cmp(item, *lower_)) return false;
        if (upper_.has_value() && cmp(*upper_, item)) return false;
        return true;
    }

    /**
     * @brief Gets the number of elements in the SortedSet, or in a view's range.
     *
     * C++ counterpart of .NET SortedSet<T>.Count. O(1) for an owning full set. For a view the
     * in-range count is an O(k) walk of the k in-range elements, computed once per version of
     * the shared state and then cached, mirroring .NET TreeSubSet's `count`/`_countVersion`
     * pair. Any mutation anywhere in the shared state bumps that version, so a cached value
     * can never be returned as current after a completed mutation.
     *
     * @par Concurrency
     * Safe to call concurrently from any number of threads, on one object or on several
     * handles, provided no thread is mutating (see the class-level thread-safety contract).
     * The cache is published with a two-step protocol rather than written plainly, which is
     * what makes that true (ticket #1784): the count is stored first, then the version is
     * stored with `release`. A reader that observes the new version with `acquire` therefore
     * also observes the matching count, so the (count, version) pair can never be read torn.
     * With no concurrent mutation every racing thread computes the *same* value for the same
     * version, so duplicate publication is harmless -- the protocol exists to order the pair,
     * not to arbitrate between conflicting results. This member allocates nothing.
     *
     * @return The number of elements.
     */
    [[nodiscard]] intcs getCountProperty() const {
        if (!getIsViewProperty()) return static_cast<intcs>(state_->data.size());
        const intcs currentVersion = state_->version;
        if (cachedCountVersion_.load(std::memory_order_acquire) == currentVersion)
            return cachedCount_.load(std::memory_order_relaxed);
        const auto computed = static_cast<intcs>(std::distance(rangeBegin(), rangeEnd()));
        cachedCount_.store(computed, std::memory_order_relaxed);
        cachedCountVersion_.store(currentVersion, std::memory_order_release);
        return computed;
    }

    /**
     * @brief Gets a value indicating whether the SortedSet (or a view's range) contains no elements.
     * @return true if empty; otherwise false.
     */
    [[nodiscard]] bool getIsEmptyProperty() const { return getCountProperty() == 0; }

    /**
     * @brief Gets the minimum value in the SortedSet, or a default-constructed T if empty.
     *
     * C++ counterpart of .NET SortedSet<T>.Min, which returns default(T) (.NET's nullable
     * T?) rather than throwing when the set is empty. On a view the minimum is scoped to the
     * view's range and found in O(log n) rather than by a tree walk.
     */
    [[nodiscard]] T getMinProperty() const {
        const auto b = rangeBegin();
        return b == rangeEnd() ? T{} : *b;
    }

    /**
     * @brief Gets the maximum value in the SortedSet, or a default-constructed T if empty.
     *
     * C++ counterpart of .NET SortedSet<T>.Max, which returns default(T) (.NET's nullable
     * T?) rather than throwing when the set is empty. On a view the maximum is scoped to the
     * view's range.
     */
    [[nodiscard]] T getMaxProperty() const {
        const auto b = rangeBegin();
        const auto e = rangeEnd();
        if (b == e) return T{};
        return *std::prev(e);
    }

    /**
     * @brief Adds the specified element to the set, writing through to the shared state.
     *
     * C++ counterpart of .NET SortedSet<T>.Add(T) and TreeSubSet.AddIfNotPresent(T).
     * @param item The element to add.
     * @return true if the element was added; false if it was already present.
     * @throws System::ArgumentOutOfRangeException if this object is a view and @p item lies
     *         outside its bounds. Nothing is written when the exception is thrown.
     */
    bool Add(const T& item) {
        if (getIsViewProperty() && !IsWithinRange(item))
            throw System::ArgumentOutOfRangeException("item");
        const bool added = state_->data.insert(item).second;
        if (added) ++state_->version;
        return added;
    }

    /**
     * @brief Removes the specified element from the set, writing through to the shared state.
     *
     * C++ counterpart of .NET SortedSet<T>.Remove(T) and TreeSubSet.DoRemove(T). An
     * out-of-range value on a view returns false without throwing and leaves the underlying
     * set untouched, matching .NET.
     * @param item The element to remove.
     * @return true if the element was found and removed; otherwise false.
     */
    bool Remove(const T& item) {
        if (getIsViewProperty() && !IsWithinRange(item)) return false;
        const bool removed = state_->data.erase(item) > 0;
        if (removed) ++state_->version;
        return removed;
    }

    /**
     * @brief Determines whether the set -- or, on a view, its range -- contains the element.
     *
     * C++ counterpart of .NET SortedSet<T>.Contains(T).
     * @param item The element to locate.
     * @return true if found within this object's bounds; otherwise false.
     */
    [[nodiscard]] bool Contains(const T& item) const {
        if (getIsViewProperty() && !IsWithinRange(item)) return false;
        return state_->data.find(item) != state_->data.end();
    }

    /**
     * @brief Removes all elements from the set, or exactly the in-range elements from a view.
     *
     * C++ counterpart of .NET SortedSet<T>.Clear() and TreeSubSet.Clear(). Clearing a view
     * removes its range from the shared underlying set and leaves everything outside the
     * bounds untouched. The version is bumped only when something was actually removed, so a
     * no-op Clear does not invalidate an in-flight enumeration.
     */
    void Clear() {
        if (!getIsViewProperty()) {
            if (!state_->data.empty()) { state_->data.clear(); ++state_->version; }
            return;
        }
        const auto b = rangeBegin();
        const auto e = rangeEnd();
        if (b == e) return;
        state_->data.erase(b, e);
        ++state_->version;
    }

    /**
     * @brief Modifies the set to contain all elements in itself and the specified set.
     *
     * C++ counterpart of .NET SortedSet<T>.UnionWith(IEnumerable<T>). Elements are added
     * through Add, so on a view the bounds are enforced and every insertion writes through to
     * the shared state. @p other contributes only the elements inside its own range.
     * @param other The set of elements to add.
     * @throws System::ArgumentOutOfRangeException if this object is a view and @p other
     *         contains a value outside its bounds; elements added before that point remain
     *         added, matching .NET's element-at-a-time behavior.
     */
    void UnionWith(const SortedSet<T>& other) {
        // Materialize other's range first: when `other` is a handle onto the same shared
        // state, inserting into it would otherwise invalidate the iterator walking it.
        const std::vector<T> source = other.ToVector();
        for (const T& x : source) Add(x);
    }

    /**
     * @brief Modifies the set to contain only elements also present in the specified set.
     *
     * C++ counterpart of .NET SortedSet<T>.IntersectWith(IEnumerable<T>). On a view only the
     * in-range elements are considered and removals write through to the shared state.
     * @param other The set to intersect with.
     */
    void IntersectWith(const SortedSet<T>& other) {
        if (aliasesSameRangeAs(other)) return;
        const std::vector<T> mine = ToVector();
        for (const T& x : mine)
            if (!other.Contains(x)) Remove(x);
    }

    /**
     * @brief Removes all elements in the specified set from the current set.
     *
     * C++ counterpart of .NET SortedSet<T>.ExceptWith(IEnumerable<T>).
     * @param other The set of elements to remove.
     */
    void ExceptWith(const SortedSet<T>& other) {
        // Shared-state self-aliasing guard. Before ticket #1783 this compared object identity
        // (`&other == this`); with reference-counted state two distinct objects can address
        // the same std::set, so erasing while iterating `other` is the same iterator-
        // invalidation UB ticket 324 fixed for HashSet<T>. Real .NET's SortedSet<T>.ExceptWith
        // has the identical `if (other == this) { Clear(); return; }` special case.
        if (aliasesSameRangeAs(other)) { Clear(); return; }
        const std::vector<T> source = other.ToVector();
        for (const T& x : source) Remove(x);
    }

    /**
     * @brief Modifies the set so it contains only elements present in one set but not both.
     *
     * C++ counterpart of .NET SortedSet<T>.SymmetricExceptWith(IEnumerable<T>). Elements are
     * added and removed through Add/Remove, so a view enforces its bounds and writes through.
     * @param other The set to compare to.
     * @throws System::ArgumentOutOfRangeException if this object is a view and @p other
     *         contributes a value outside its bounds.
     */
    void SymmetricExceptWith(const SortedSet<T>& other) {
        // Same self-aliasing hazard and fix as ExceptWith above -- real .NET's
        // SortedSet<T>.SymmetricExceptWith also special-cases `other == this`.
        if (aliasesSameRangeAs(other)) { Clear(); return; }
        const std::vector<T> source = other.ToVector();
        for (const T& x : source) {
            if (Contains(x)) Remove(x);
            else Add(x);
        }
    }

    /**
     * @brief Determines whether the set is a subset of the specified set.
     *
     * C++ counterpart of .NET SortedSet<T>.IsSubsetOf(IEnumerable<T>). Only the in-range
     * elements of each object take part.
     * @param other The set to compare to.
     * @return true if every in-range element of this object is in @p other's range.
     */
    [[nodiscard]] bool IsSubsetOf(const SortedSet<T>& other) const {
        for (auto it = rangeBegin(); it != rangeEnd(); ++it)
            if (!other.Contains(*it)) return false;
        return true;
    }

    /**
     * @brief Determines whether the set is a superset of the specified set.
     *
     * C++ counterpart of .NET SortedSet<T>.IsSupersetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if every in-range element of @p other is in this object's range.
     */
    [[nodiscard]] bool IsSupersetOf(const SortedSet<T>& other) const {
        return other.IsSubsetOf(*this);
    }

    /**
     * @brief Determines whether the set is a proper (strict) subset of the specified set.
     *
     * C++ counterpart of .NET SortedSet<T>.IsProperSubsetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if this object is a subset of @p other and the two are not equal.
     */
    [[nodiscard]] bool IsProperSubsetOf(const SortedSet<T>& other) const {
        return IsSubsetOf(other) && !SetEquals(other);
    }

    /**
     * @brief Determines whether the set is a proper (strict) superset of the specified set.
     *
     * C++ counterpart of .NET SortedSet<T>.IsProperSupersetOf(IEnumerable<T>).
     * @param other The set to compare to.
     * @return true if this object is a superset of @p other and the two are not equal.
     */
    [[nodiscard]] bool IsProperSupersetOf(const SortedSet<T>& other) const {
        return IsSupersetOf(other) && !SetEquals(other);
    }

    /**
     * @brief Determines whether the set and the specified set contain the same elements.
     *
     * C++ counterpart of .NET SortedSet<T>.SetEquals(IEnumerable<T>). The two ranges are
     * compared element-wise through comparer equivalence (`!cmp(a,b) && !cmp(b,a)`) rather
     * than through operator== on T or on the whole container, so it is correct for a view and
     * requires nothing of T beyond the ordering std::set already uses.
     * @param other The set to compare to.
     * @return true if the two ranges are equal; otherwise false.
     */
    [[nodiscard]] bool SetEquals(const SortedSet<T>& other) const {
        auto a = rangeBegin();
        const auto ae = rangeEnd();
        auto b = other.rangeBegin();
        const auto be = other.rangeEnd();
        const auto cmp = comparer();
        for (; a != ae && b != be; ++a, ++b)
            if (cmp(*a, *b) || cmp(*b, *a)) return false;
        return a == ae && b == be;
    }

    /**
     * @brief Determines whether the set and the specified set share any common elements.
     *
     * C++ counterpart of .NET SortedSet<T>.Overlaps(IEnumerable<T>). Only the in-range
     * elements of each object take part.
     * @param other The set to compare to.
     * @return true if at least one element is common to both ranges; otherwise false.
     */
    [[nodiscard]] bool Overlaps(const SortedSet<T>& other) const {
        for (auto it = other.rangeBegin(); it != other.rangeEnd(); ++it)
            if (Contains(*it)) return true;
        return false;
    }

    /**
     * @brief Returns a live, bounded, bidirectionally write-through view of the elements in
     *        the inclusive range [@p lower, @p upper].
     *
     * C++ counterpart of .NET SortedSet<T>.GetViewBetween(T, T), which returns a
     * `TreeSubSet` over the same underlying tree.
     *
     * The returned SortedSet<T> is **not** a copy. It shares this object's underlying tree
     * state, so an in-range element added to or removed from the view appears in (or
     * disappears from) this set and every other view over the same state, and mutations made
     * here are visible through the view. The view keeps the shared state alive, so it stays
     * valid even after this object is destroyed, copied, moved, or reassigned.
     *
     * The view enforces its bounds for the whole of its life: an out-of-range Add throws
     * System::ArgumentOutOfRangeException, an out-of-range Remove returns false, an
     * out-of-range Contains returns false, and Clear removes exactly the range. A nested
     * GetViewBetween may only **narrow** the current bounds, and is flattened onto the same
     * root state rather than building a chain of views, exactly as .NET's
     * `TreeSubSet::GetViewBetween` delegates to `_underlying.GetViewBetween`.
     *
     * @warning INTENTIONAL BREAKING CHANGE (ticket #1783, SR-AUD-361). Earlier revisions
     * returned an independent snapshot copy and this member was `const`. Code that mutated
     * the result expecting the source to be unaffected now mutates the source. Call
     * ToSortedSet() on the result to obtain the old detached behavior deliberately -- the C++
     * spelling of .NET's `new SortedSet<T>(view)`. The member is no longer `const`, because a
     * live view is a mutable path into the object it was taken from; a `const` set cannot
     * produce one.
     *
     * @param lower The minimum value (inclusive).
     * @param upper The maximum value (inclusive).
     * @return A live SortedSet<T> handle bounded by [@p lower, @p upper].
     * @throws System::ArgumentException if @p lower orders after @p upper.
     * @throws System::ArgumentOutOfRangeException with parameter name `lowerValue` or
     *         `upperValue` if this object is already a view and the requested bound would
     *         widen it.
     */
    [[nodiscard]] SortedSet<T> GetViewBetween(const T& lower, const T& upper) {
        const auto cmp = comparer();
        if (cmp(upper, lower))
            throw System::ArgumentException("Must be less than or equal to upperValue.", "lowerValue");
        // A nested view may only narrow, never widen -- .NET TreeSubSet::GetViewBetween.
        if (lower_.has_value() && cmp(lower, *lower_))
            throw System::ArgumentOutOfRangeException("lowerValue");
        if (upper_.has_value() && cmp(*upper_, upper))
            throw System::ArgumentOutOfRangeException("upperValue");
        // Flattened onto the root state: nesting depth is always 1.
        return SortedSet<T>(state_, std::optional<T>(lower), std::optional<T>(upper));
    }

    /**
     * @brief Materializes an independent, unbounded SortedSet holding this object's current
     *        in-range elements.
     *
     * The C++ spelling of .NET's `new SortedSet<T>(view)`, and the supported way to obtain a
     * detached point-in-time copy of a view (or of a set) after ticket #1783 made
     * GetViewBetween live. A collection constructor cannot express this because it would
     * collide with the copy constructor, whose role-preserving behavior must be kept.
     * @return A new owning SortedSet<T> that shares nothing with this object.
     */
    [[nodiscard]] SortedSet<T> ToSortedSet() const {
        SortedSet<T> result;
        result.state_->data.insert(rangeBegin(), rangeEnd());
        return result;
    }

    /**
     * @brief Copies the elements -- or, on a view, the in-range elements -- to a new vector in
     *        sorted order.
     *
     * C++ counterpart of .NET SortedSet<T>.CopyTo(T[]).
     * @return A std::vector<T> containing this object's elements in ascending order.
     */
    [[nodiscard]] std::vector<T> ToVector() const {
        return std::vector<T>(rangeBegin(), rangeEnd());
    }

    /**
     * @brief Returns a version-checked iterator to the first in-range element (range-for).
     * @throws System::InvalidOperationException from dereference/increment if the shared state
     *         is structurally modified while this iterator is in use -- including a
     *         modification made through a different view or through the owning set.
     */
    Iterator begin() const { return Iterator(state_, rangeBegin(), rangeEnd()); }
    /** @brief Returns a version-checked iterator past the last in-range element (range-for). */
    Iterator end()   const { return Iterator(state_, rangeEnd(),   rangeEnd()); }
};

} // namespace System::Collections::Generic
