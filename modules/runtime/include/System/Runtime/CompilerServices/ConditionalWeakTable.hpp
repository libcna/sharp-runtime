// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Collections/Generic/IEnumerable.hpp"
#include "System/Collections/Generic/KeyValuePair.hpp"

namespace System::Runtime::CompilerServices {

/**
 * Associates values with object keys without keeping those keys alive.
 *
 * C++ counterpart of .NET System.Runtime.CompilerServices.ConditionalWeakTable<TKey,TValue>.
 * Keys and values use `std::shared_ptr`: keys are stored as `std::weak_ptr`, while values remain
 * strongly owned until their key expires. Consequently, callers must retain a `std::shared_ptr`
 * to each live key. All public operations are safe for concurrent callers.
 *
 * @tparam TKey Type of the object used as a weakly held key.
 * @tparam TValue Type of the value associated with a key.
 *
 * @note **Deliberate, permanent widening of the generic domain.** .NET constrains both type
 * parameters with `where T : class`, so `ConditionalWeakTable<int, int>` fails to compile there
 * (CS0452). This port applies **no** such constraint, and `ConditionalWeakTable<int, int>` is a
 * supported instantiation. That is not an oversight and must not be "fixed" by adding a
 * constraint: the managed rule exists because the **CLR cannot create a weak GC handle to a value
 * type**, and this port does not create GC handles at all. It keys on `std::weak_ptr<TKey>` and
 * stores `std::shared_ptr<TValue>`, and `std::weak_ptr<int>` is a perfectly well-defined
 * reference to a heap-managed control block with exactly the expiry semantics this table needs.
 * Adopting the constraint would delete working, well-defined functionality in order to imitate a
 * restriction whose cause does not exist here.
 *
 * The consequence a caller should know: the *pointer identity* of the `shared_ptr`'s control
 * block is the key, not the pointed-to value, so two `shared_ptr<int>` objects holding equal
 * integers are two different keys. That is the same identity rule the managed API uses for
 * reference types, and it is why scalar `TKey` works rather than being a special case.
 *
 * Recorded for ticket #1982 / SR-AUD-162; see `docs/SystemRuntimeNamespaceReviewPlan.md` §4.6.
 * It would be reopened by evidence that `weak_ptr<TKey>` keying has a semantic defect for scalar
 * `TKey` — not merely by the existence of CS0452.
 */
template<typename TKey, typename TValue>
class ConditionalWeakTable final
    : public System::Collections::Generic::IEnumerable<
          System::Collections::Generic::KeyValuePair<std::shared_ptr<TKey>, std::shared_ptr<TValue>>> {
public:
    /** Shared ownership type used for keys. */
    using KeyPtr = std::shared_ptr<TKey>;

    /** Shared ownership type used for values. A null value is permitted. */
    using ValuePtr = std::shared_ptr<TValue>;

    /** Pair type returned by the table enumerator. */
    using Pair = System::Collections::Generic::KeyValuePair<KeyPtr, ValuePtr>;

    /** Callback used by GetValue to construct a value for a missing key. */
    using CreateValueCallback = std::function<ValuePtr(const KeyPtr&)>;

private:
    struct Entry {
        std::weak_ptr<TKey> key;
        ValuePtr value;
    };

    /**
     * @brief One row of an enumerator's snapshot: two WEAK references.
     *
     * Ticket #1981 (SR-AUD, cause R-H). The snapshot used to be a `std::vector<Entry>`, and
     * `Entry::value` is a **strong** `shared_ptr`, so an enumerator kept every value it had
     * snapshotted alive for its own lifetime — including values the table had since released.
     * Measured (`build-probe/1981_probe1_layout.cpp`): after `table.Remove(key)` the value was
     * still alive, and became collectable only when the enumerator was deleted.
     *
     * .NET's enumerator retains only `Current` (`ConditionalWeakTable.cs:441-490`).
     */
    struct SnapshotEntry {
        std::weak_ptr<TKey>   key;
        std::weak_ptr<TValue> value;
    };

    class Enumerator final : public System::Collections::Generic::IEnumerator<Pair> {
        std::vector<SnapshotEntry> entries_;
        std::size_t nextIndex_ = 0;
        Pair current_;
        bool hasCurrent_ = false;

    public:
        /** Initializes an enumerator over the table entries visible at creation time. */
        explicit Enumerator(std::vector<SnapshotEntry> entries) : entries_(std::move(entries)) {}

        /**
         * @brief Advances to the next entry whose key AND value are both still alive.
         *
         * Both halves are locked here rather than snapshotted strongly, so the only value this
         * enumerator keeps alive is the one in `current_` — which is what .NET retains too.
         * An entry the table released after the snapshot was taken is skipped, exactly as
         * .NET's `TryGetEntry` loop skips one that has been removed or collected
         * (`ConditionalWeakTable.cs:459-467`).
         */
        bool MoveNext() override {
            while (nextIndex_ < entries_.size()) {
                const SnapshotEntry& entry = entries_[nextIndex_++];
                KeyPtr key = entry.key.lock();
                ValuePtr value = entry.value.lock();
                if (key && value) {
                    current_ = Pair(std::move(key), std::move(value));
                    hasCurrent_ = true;
                    return true;
                }
            }

            current_ = Pair{};
            hasCurrent_ = false;
            return false;
        }

        /**
         * @brief Does nothing, matching .NET.
         *
         * `ConditionalWeakTable.cs:492` is literally `public void Reset() { }`. This port used to
         * rewind the snapshot index and clear `Current`, so a caller could re-enumerate; that is
         * a capability .NET's enumerator does not have, and ticket #1981 removes it deliberately.
         *
         * Because the body is empty, `Current()` after `Reset()` still returns the last element
         * — again matching .NET, whose `Current` is guarded on `_currentIndex < 0` and is
         * therefore unaffected by a call that changes nothing.
         */
        void Reset() override {}

        /** Gets the current key/value pair. */
        [[nodiscard]] const Pair& Current() const override {
            if (!hasCurrent_) {
                throw System::InvalidOperationException(
                    "Enumeration has either not started or has already finished.");
            }
            return current_;
        }
    };

    mutable std::mutex mutex_;
    std::vector<Entry> entries_;

    static void requireKey(const KeyPtr& key) {
        if (!key) {
            throw System::ArgumentNullException("key");
        }
    }

    static void requireCallback(const CreateValueCallback& callback, const char* parameterName) {
        if (!callback) {
            throw System::ArgumentNullException(parameterName);
        }
    }

    void removeExpiredLocked() {
        std::erase_if(entries_, [](const Entry& entry) { return entry.key.expired(); });
    }

    [[nodiscard]] auto findEntryLocked(const KeyPtr& key) {
        return std::find_if(entries_.begin(), entries_.end(), [&key](const Entry& entry) {
            KeyPtr existing = entry.key.lock();
            return existing && existing.get() == key.get();
        });
    }

    [[nodiscard]] ValuePtr getOrAddLocked(const KeyPtr& key, const ValuePtr& value) {
        std::lock_guard lock(mutex_);
        removeExpiredLocked();
        const auto entry = findEntryLocked(key);
        if (entry != entries_.end()) {
            return entry->value;
        }

        entries_.push_back(Entry{key, value});
        return value;
    }

public:
    /** Initializes an empty conditional weak table. */
    ConditionalWeakTable() = default;

    ConditionalWeakTable(const ConditionalWeakTable&) = delete;
    ConditionalWeakTable& operator=(const ConditionalWeakTable&) = delete;
    ConditionalWeakTable(ConditionalWeakTable&&) = delete;
    ConditionalWeakTable& operator=(ConditionalWeakTable&&) = delete;

    /**
     * Gets the value associated with a key.
     * @param key Key to locate; must not be null.
     * @param value Receives the associated value, or null when no entry exists.
     * @return true when an entry was found; otherwise false.
     */
    [[nodiscard]] bool TryGetValue(const KeyPtr& key, ValuePtr& value) {
        requireKey(key);
        std::lock_guard lock(mutex_);
        removeExpiredLocked();
        const auto entry = findEntryLocked(key);
        if (entry == entries_.end()) {
            value.reset();
            return false;
        }
        value = entry->value;
        return true;
    }

    /**
     * Adds a key and value pair.
     * @throws System::ArgumentNullException when @p key is null.
     * @throws System::ArgumentException when @p key is already present.
     */
    void Add(const KeyPtr& key, const ValuePtr& value) {
        requireKey(key);
        std::lock_guard lock(mutex_);
        removeExpiredLocked();
        if (findEntryLocked(key) != entries_.end()) {
            throw System::ArgumentException("An item with the same key has already been added.");
        }
        entries_.push_back(Entry{key, value});
    }

    /**
     * Adds a key and value pair only when the key is absent.
     * @return true when the pair was added; false when an entry already existed.
     */
    [[nodiscard]] bool TryAdd(const KeyPtr& key, const ValuePtr& value) {
        requireKey(key);
        std::lock_guard lock(mutex_);
        removeExpiredLocked();
        if (findEntryLocked(key) != entries_.end()) {
            return false;
        }
        entries_.push_back(Entry{key, value});
        return true;
    }

    /** Adds a missing key or updates the value associated with an existing key. */
    void AddOrUpdate(const KeyPtr& key, const ValuePtr& value) {
        requireKey(key);
        std::lock_guard lock(mutex_);
        removeExpiredLocked();
        const auto entry = findEntryLocked(key);
        if (entry == entries_.end()) {
            entries_.push_back(Entry{key, value});
        } else {
            entry->value = value;
        }
    }

    /** Removes a key and its value from the table. */
    [[nodiscard]] bool Remove(const KeyPtr& key) {
        ValuePtr ignored;
        return Remove(key, ignored);
    }

    /**
     * Removes a key and returns its value when present.
     * @param value Receives the removed value, or null when the key is absent.
     */
    [[nodiscard]] bool Remove(const KeyPtr& key, ValuePtr& value) {
        requireKey(key);
        std::lock_guard lock(mutex_);
        removeExpiredLocked();
        const auto entry = findEntryLocked(key);
        if (entry == entries_.end()) {
            value.reset();
            return false;
        }
        value = std::move(entry->value);
        entries_.erase(entry);
        return true;
    }

    /** Removes all pairs from the table. */
    void Clear() {
        std::lock_guard lock(mutex_);
        entries_.clear();
    }

    /** Returns the existing value for a key, or inserts and returns @p value. */
    [[nodiscard]] ValuePtr GetOrAdd(const KeyPtr& key, const ValuePtr& value) {
        requireKey(key);
        ValuePtr existing;
        if (TryGetValue(key, existing)) {
            return existing;
        }
        return getOrAddLocked(key, value);
    }

    /**
     * Returns the existing value for a key, or creates one outside the table lock.
     *
     * As in .NET, contending calls may invoke the factory more than once, but only the value
     * from the winning insertion is retained and returned.
     */
    [[nodiscard]] ValuePtr GetOrAdd(const KeyPtr& key, const CreateValueCallback& valueFactory) {
        requireKey(key);
        requireCallback(valueFactory, "valueFactory");
        ValuePtr existing;
        if (TryGetValue(key, existing)) {
            return existing;
        }
        return getOrAddLocked(key, valueFactory(key));
    }

    /**
     * Returns the existing value for a key, or creates one with an additional factory argument.
     * @tparam TArg Type of the extra factory argument.
     */
    template<typename TArg>
    [[nodiscard]] ValuePtr GetOrAdd(
        const KeyPtr& key,
        const std::function<ValuePtr(const KeyPtr&, const TArg&)>& valueFactory,
        const TArg& factoryArgument) {
        requireKey(key);
        if (!valueFactory) {
            throw System::ArgumentNullException("valueFactory");
        }
        ValuePtr existing;
        if (TryGetValue(key, existing)) {
            return existing;
        }
        return getOrAddLocked(key, valueFactory(key, factoryArgument));
    }

    /** Returns the existing value for a key, or creates one using the supplied legacy callback. */
    [[nodiscard]] ValuePtr GetValue(const KeyPtr& key, const CreateValueCallback& createValueCallback) {
        requireKey(key);
        requireCallback(createValueCallback, "createValueCallback");
        ValuePtr existing;
        if (TryGetValue(key, existing)) {
            return existing;
        }
        return getOrAddLocked(key, createValueCallback(key));
    }

    /** Returns the existing value for a key, or default-constructs a value for a missing key. */
    [[nodiscard]] ValuePtr GetOrCreateValue(const KeyPtr& key) {
        return GetValue(key, [](const KeyPtr&) { return std::make_shared<TValue>(); });
    }

    /**
     * Gets an enumerator over entries visible when this method is called.
     * Expired keys are skipped and entries added later are not enumerated.
     */
    [[nodiscard]] System::Collections::Generic::IEnumerator<Pair>* GetEnumerator() override {
        std::lock_guard lock(mutex_);
        removeExpiredLocked();
        // #1981: the snapshot demotes each value to a weak reference, so the enumerator retains
        // only whatever `Current` holds. Copying `entries_` directly would copy a strong
        // ValuePtr per row, which is the over-retention this ticket removes.
        std::vector<SnapshotEntry> snapshot;
        snapshot.reserve(entries_.size());
        for (const Entry& entry : entries_) {
            snapshot.push_back(SnapshotEntry{entry.key, std::weak_ptr<TValue>(entry.value)});
        }
        return new Enumerator(std::move(snapshot));
    }
};

} // namespace System::Runtime::CompilerServices
