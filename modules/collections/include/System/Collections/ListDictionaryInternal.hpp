// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <list>
#include <stdexcept>
#include <vector>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"
#include "System/Collections/detail/MutationCounter.hpp"

namespace System::Collections {

using SharpRuntime::intcs;

/**
 * @brief Implements IDictionary using a singly linked list.
 *
 * C++ counterpart of .NET System.Collections.ListDictionaryInternal.
 * Recommended for collections that typically include fewer than 10 items.
 *
 * @warning Keys are compared by pointer identity (`==` on `const void*`), not by value.
 * Real .NET compares keys with `key.Equals(other)` (ListDictionaryInternal.cs), so e.g. two
 * distinct `string` objects with equal contents are the same key in .NET but distinct keys
 * here. This is a permanent architectural limitation, not a bug to be fixed locally: C++ has
 * no common object root, so this non-generic, `const void*`-keyed type cannot call a virtual
 * `Equals` on an arbitrary key without knowing its concrete type (same root cause as
 * System::Collections::Comparer and System::Collections::StructuralComparisons). Callers
 * needing value-based key comparison should use a generic, typed dictionary instead.
 *
 * @note **A null key is rejected on every raw-key entry point** --
 * getItem/setItem/Add/Contains/Remove all throw System::ArgumentNullException("key")
 * before they look at storage, matching .NET's `ArgumentNullException.ThrowIfNull(key)`
 * and the sibling System::Collections::Hashtable. Validation is structural, not
 * conventional: the private locator accepts only a ValidatedKey, and the only way to
 * obtain one is to pass the null check.
 *
 * @note **The mutation counter advances on each effective mutation and on nothing
 * else** -- on an insert, on a replacement (including an equal-value replacement),
 * and on a successful Remove, but not on a duplicate Add that throws, not on a Remove
 * of an absent key, and never on a rejected null key. Clear is the single deliberate
 * exception and bumps unconditionally, matching .NET. This port therefore does **not**
 * copy .NET ListDictionaryInternal's unconditional bump-first shape, which would
 * invalidate outstanding enumerators after operations that changed nothing; the rule
 * followed here is .NET Hashtable's, and it is the rule both of this port's
 * IDictionary implementations use. Full record and measurements:
 * `docs/ListDictionaryInternalSetterDesign.md` (design #1799, implementation #1798).
 */
class ListDictionaryInternal : public IDictionary {
    struct Node {
        const void* key;
        void*       value;
    };
    std::list<Node> list_;
    System::Collections::detail::MutationCounter version_;

    /**
     * @brief A key that has passed the null check -- the only type the locator accepts.
     *
     * Constructing one is the single place a raw `const void*` key is validated,
     * and the private locator accepts nothing else, so a future public key-taking
     * method physically cannot reach `list_` without validating first. That is
     * stronger than Hashtable's `toKey()`, which is a convention every entry point
     * must remember to follow, and stronger than five scattered `if (key == nullptr)`
     * checks, where a sixth entry point silently reopens the defect (ticket #1798,
     * `docs/ListDictionaryInternalSetterDesign.md` sections 14.1 and 33).
     *
     * It costs no allocation and no stringification: unlike Hashtable's `toKey()`,
     * which builds a std::string per raw-key call, this is one null compare and one
     * pointer copy, and it emits no symbol at all at -O2.
     */
    class ValidatedKey {
        const void* key_;

    public:
        /** @throws System::ArgumentNullException if @p key is null. */
        explicit ValidatedKey(const void* key) : key_(key) {
            if (key == nullptr) throw System::ArgumentNullException("key");
        }
        /** @brief The validated, non-null key pointer. */
        [[nodiscard]] const void* getValueProperty() const noexcept { return key_; }
    };

    /** @brief The single lookup path; compares by address, as the class warning states. */
    [[nodiscard]] std::list<Node>::const_iterator findNode(ValidatedKey key) const {
        for (auto it = list_.begin(); it != list_.end(); ++it)
            if (it->key == key.getValueProperty()) return it;
        return list_.end();
    }
    /** @brief Non-const overload of the single lookup path. */
    [[nodiscard]] std::list<Node>::iterator findNode(ValidatedKey key) {
        for (auto it = list_.begin(); it != list_.end(); ++it)
            if (it->key == key.getValueProperty()) return it;
        return list_.end();
    }

    /**
     * Test-only seam (declared in detail/MutationCounter.hpp, never defined in
     * production) letting a regression position the mutation counter near a boundary.
     */
    friend struct SharpRuntime::Testing::CollectionVersionAccess<ListDictionaryInternal>;

    // Shared forward enumerator over list_'s Nodes; getEntryProperty()/getKeyProperty()/
    // getValueProperty() expose the current entry, matching IDictionaryEnumerator.
    class NodeEnumerator : public IDictionaryEnumerator {
        const ListDictionaryInternal* d_;
        System::Collections::detail::MutationVersion version_;
        std::list<Node>::const_iterator it_;
        bool started_ = false;
        bool valid_   = false;
        /**
         * Snapshot of the entry `it_` names, refreshed by a successful MoveNext()
         * and read by all four accessors, so that none of them dereferences `it_`.
         * Added by ticket #1794: this class previously rebuilt every accessor's
         * answer from `it_` on each call, which made all four -- including the
         * already-owning getEntryProperty() and getCurrentProperty() -- an
         * AddressSanitizer heap-use-after-free once a mutation, Clear(), or the
         * dictionary's destruction had invalidated the iterator. .NET's own
         * NodeEnumerator holds a strong reference to the node instead, a
         * guarantee a C++ port with no GC cannot inherit; copying is the
         * equivalent. Costs +32 bytes on a PRIVATE nested class (40 -> 72).
         */
        DictionaryEntry current_;

    public:
        explicit NodeEnumerator(const ListDictionaryInternal* d)
            : d_(d), version_(d->version_), it_(d->list_.begin()) {}

        bool MoveNext() override {
            if (version_ != d_->version_) throw System::InvalidOperationException("Collection was modified; enumeration operation may not execute.");
            if (!started_) { it_ = d_->list_.begin(); started_ = true; }
            else if (valid_) { ++it_; }
            valid_ = (it_ != d_->list_.end());
            // Refresh the snapshot only after the version check has passed and
            // only when positioned; a MoveNext() that returns false or throws
            // publishes nothing and leaves the accessors to requireValid().
            if (valid_) current_ = DictionaryEntry(it_->key, it_->value);
            else        current_ = DictionaryEntry();
            return valid_;
        }

        void Reset() override {
            if (version_ != d_->version_) throw System::InvalidOperationException("Collection was modified; enumeration operation may not execute.");
            started_ = false;
            valid_ = false;
            it_ = d_->list_.begin();
            current_ = DictionaryEntry();
        }

        /**
         * @brief Returns a copy of the current entry, boxed as std::any(DictionaryEntry).
         *
         * Recover it with std::any_cast<DictionaryEntry>. Ticket #1794 corrected
         * the payload: this boxed the KEY alone, where .NET's NodeEnumerator is
         * `public object Current => Entry;` and this repository's own
         * Hashtable::Enumerator already boxed the DictionaryEntry. The two
         * implementations of one interface therefore disagreed about what Current
         * meant. The signature and ownership contract ticket #1793 set are
         * unchanged.
         */
        [[nodiscard]] std::any getCurrentProperty() const override {
            requireValid();
            return std::any(current_);
        }

        /** @brief Returns a copy of the MoveNext()-time entry snapshot. */
        [[nodiscard]] DictionaryEntry getEntryProperty() const override {
            requireValid();
            return current_;
        }

        /**
         * @brief Returns the current entry's key, boxed as std::any(const void*).
         *
         * The `const` survives the boxing, so recover it with
         * std::any_cast<const void*>. The box owns the pointer, not the pointee:
         * the pointee is the caller's own object, which the caller already had.
         * Before ticket #1793 this const_cast'd the key, publishing a writable
         * pointer to an object the *caller* owns and declared const.
         */
        [[nodiscard]] std::any getKeyProperty() const override {
            requireValid();
            return current_.getKeyProperty();
        }

        /**
         * @brief Returns the current entry's value, boxed as std::any(void*).
         *
         * Recover it with std::any_cast<void*>. `void*` rather than `const void*`
         * is what this dictionary genuinely stores -- Add(const void*, void*) and
         * getItem() both say so -- and it is what DictionaryEntry::Value has
         * always held; ticket #1794 made the value view agree with both, where it
         * previously agreed with neither.
         */
        [[nodiscard]] std::any getValueProperty() const override {
            requireValid();
            return current_.getValueProperty();
        }

    private:
        void requireValid() const {
            if (!started_ || !valid_)
                throw System::InvalidOperationException("Enumeration has either not started or has already finished.");
        }
    };

    // Read-only ICollection view over either the keys or the values of the dictionary.
    class MemberCollection : public ICollection {
        const ListDictionaryInternal* d_;
        bool keys_; // true: enumerate keys; false: enumerate values

        class Enumerator : public IEnumerator {
            IDictionaryEnumerator* inner_;
            bool keys_;
        public:
            Enumerator(IDictionaryEnumerator* inner, bool keys) : inner_(inner), keys_(keys) {}
            ~Enumerator() override { delete inner_; }
            bool MoveNext() override { return inner_->MoveNext(); }
            void Reset() override { inner_->Reset(); }
            /**
             * @brief Returns the current key or value, boxed by the inner accessor.
             *
             * The key view boxes `const void*`, preserving the `const` the caller
             * declared; recover it with std::any_cast<const void*>. The value view
             * boxes `void*` -- recovered with std::any_cast<void*> -- which since
             * ticket #1794 agrees with both DictionaryEntry::Value and copyToCore
             * below, where it previously agreed with neither and was the only one
             * of the three spellings to add a `const` the dictionary never stored.
             * Since ticket #1798 copyToCore agrees on the KEY as well, so this view
             * no longer boxes two incompatible element types depending on whether a
             * caller enumerated it or copied out of it.
             */
            [[nodiscard]] std::any getCurrentProperty() const override {
                return keys_ ? inner_->getKeyProperty() : inner_->getValueProperty();
            }
        };

    public:
        MemberCollection(const ListDictionaryInternal* d, bool keys) : d_(d), keys_(keys) {}

        [[nodiscard]] intcs getCountProperty() const override { return d_->getCountProperty(); }

        [[nodiscard]] bool getIsSynchronizedProperty() const override { return false; }
        [[nodiscard]] const void* getSyncRootProperty() const override { return d_; }

        [[nodiscard]] IEnumerator* GetEnumerator() override {
            return new Enumerator(new NodeEnumerator(d_), keys_);
        }

    protected:
        // Boxes each key as std::any(const void*) and each value as std::any(void*)
        // into the destination that ICollection::CopyTo already validated, so a key
        // is recovered with std::any_cast<const void*> and a value with
        // std::any_cast<void*> -- the same spelling as this view's own Current, as
        // the enumerator's Key/Value, as DictionaryEntry, and as the typed CopyTo.
        //
        // Ticket #1798 deleted a const_cast<void*> here. The rationale it carried --
        // normalise keys to void* "so that a caller of either view retrieves every
        // slot the same way" -- was wrong on its own terms: it bought uniformity
        // BETWEEN the key and value views at the cost of uniformity WITHIN the key
        // view, whose enumerator has boxed const void* since ticket #1793. Writing
        // through the writable pointer this manufactured for a key the caller had
        // declared const was reproduced as an AddressSanitizer SEGV on a write to
        // read-only storage (docs/ListDictionaryInternalSetterDesign.md section 8.3);
        // the library, not the caller, had removed the guarantee.
        //
        // The view is private and only ever escapes as an ICollection*, so it needs
        // no typed overload of its own -- getCountProperty() plus the fixed std::any
        // element type is enough for a getKeysProperty()/getValuesProperty() consumer
        // to allocate a correct destination, which the raw void* boundary never was.
        void copyToCore(ObjectSpan destination, intcs index) override {
            intcs i = index;
            for (const auto& n : d_->list_)
                destination[i++] = keys_ ? std::any(n.key) : std::any(n.value);
        }
    };

public:
    /** @brief Initializes a new empty ListDictionaryInternal. */
    ListDictionaryInternal() = default;

    /**
     * @brief Gets the number of key/value pairs contained in the dictionary.
     *
     * C++ counterpart of .NET ListDictionaryInternal.Count.
     */
    [[nodiscard]] intcs getCountProperty() const override {
        return static_cast<intcs>(list_.size());
    }

    /** @brief Keeps the inherited validating CopyTo overloads visible next to the typed one. */
    using ICollection::CopyTo;

    /**
     * @brief Copies dictionary entries (as DictionaryEntry) into @p destination starting at @p index.
     *
     * C++ counterpart of .NET ICollection.CopyTo(Array, int). The destination
     * carries its own length, so the copy is bounds-checked before the first write.
     * @param destination Destination vector, sized by the caller; never resized here.
     * @param index       Zero-based index at which copying begins.
     * @throws System::ArgumentOutOfRangeException if @p index is negative.
     * @throws System::ArgumentException           if @p destination cannot hold
     *         getCountProperty() elements starting at @p index -- including a
     *         non-empty collection copied into a zero-length destination.
     * @throws System::ArgumentNullException       if @p destination has a null
     *         pointer and a positive length -- a null pointer paired with a zero
     *         length is a valid empty destination.
     */
    void CopyTo(std::vector<DictionaryEntry>& destination, intcs index) {
        detail::requireValidCopyDestination(destination, index, getCountProperty());
        intcs i = index;
        for (const auto& n : list_)
            destination[static_cast<size_t>(i++)] = DictionaryEntry(n.key, n.value);
    }

    /**
     * @brief Gets the value associated with the specified key, or an empty std::any.
     *
     * C++ counterpart of .NET ListDictionaryInternal indexer getter.
     *
     * @param key Pointer used as the key (compared by address). Never null.
     * @return An owning std::any boxing the stored `void*`, recovered with
     *         std::any_cast&lt;void*&gt;, or an empty std::any if the key is absent.
     * @throws System::ArgumentNullException if @p key is null.
     *
     * @note Ticket #1796 migrated this member mechanically, because
     *       IDictionary::getItem's return type changed and a pure virtual forces
     *       every implementation. **The semantics are deliberately unchanged**: the
     *       box holds the caller's own pointer, exactly as the raw `void*` return
     *       did, and this dictionary stores no value data of its own to alias. Only
     *       the return type moved.
     * @note An absent key and a key present with a null *value* stay distinguishable
     *       here, unlike Hashtable: the first reads as an empty std::any, the second
     *       as a std::any holding a null `void*`.
     * @note Ticket #1798 added the null-key rejection. Reading never advances the
     *       mutation counter, so an outstanding enumerator is unaffected by this call
     *       whether or not the key is present, and a rejected null key leaves Count,
     *       the counter and every entry untouched.
     */
    [[nodiscard]] std::any getItem(const void* key) const override {
        const auto it = findNode(ValidatedKey(key));
        return it == list_.end() ? std::any{} : std::any(it->value);
    }

    /**
     * @brief Sets the value for the key, adding a new entry if the key is not present.
     *
     * C++ counterpart of .NET ListDictionaryInternal indexer setter. One upsert path:
     * validate, locate, then replace-and-bump or insert-and-bump.
     *
     * @param key   Pointer used as the key (compared by address). Never null.
     * @param value Value to associate with the key.
     * @throws System::ArgumentNullException if @p key is null -- thrown before the
     *         lookup, so nothing is mutated and the counter does not move.
     *
     * @note **A replacement advances the mutation counter, including an equal-value
     *       replacement**, so an outstanding enumerator fails fast afterwards. Before
     *       ticket #1798 the replace branch returned before `++version_`, and four
     *       enumerator kinds -- this dictionary's IDictionaryEnumerator, the key view,
     *       the value view, and the same reached through an `IDictionary&` -- walked to
     *       the end after a replacement with no diagnostic; the value view enumerated
     *       the post-mutation value. The value is never compared: equality of a `void*`
     *       is address equality, and .NET compares neither.
     * @note The bump is placed **after** the mutation, which .NET's bump-first indexer
     *       setter cannot do. A failed insertion (a throwing `std::list::push_back`)
     *       therefore leaves the dictionary and the counter exactly as they were --
     *       a strong exception guarantee.
     */
    void setItem(const void* key, void* value) override {
        const ValidatedKey k(key);              // 1. validate, before any mutation
        const auto it = findNode(k);            // 2. locate
        if (it != list_.end()) {                // 3a. REPLACE -- bumps, incl. equal value
            it->value = value;
            ++version_;
            return;
        }
        list_.push_back({k.getValueProperty(), value});  // 3b. INSERT -- may throw
        ++version_;                                      // 4. bump only after success
    }

    /**
     * @brief Gets an ICollection view over the dictionary's keys.
     *
     * C++ counterpart of .NET IDictionary.Keys.
     * @return Heap-allocated ICollection; caller takes ownership.
     */
    [[nodiscard]] ICollection* getKeysProperty() const override { return new MemberCollection(this, true); }

    /**
     * @brief Gets an ICollection view over the dictionary's values.
     *
     * C++ counterpart of .NET IDictionary.Values.
     * @return Heap-allocated ICollection; caller takes ownership.
     */
    [[nodiscard]] ICollection* getValuesProperty() const override { return new MemberCollection(this, false); }

    /**
     * @brief Determines whether the dictionary contains an element with the specified key.
     *
     * C++ counterpart of .NET IDictionary.Contains(object).
     * @param key Pointer used as the key (compared by address). Never null.
     * @throws System::ArgumentNullException if @p key is null.
     * @note Never advances the mutation counter, so an outstanding enumerator is
     *       unaffected -- including by a rejected null key.
     */
    [[nodiscard]] bool Contains(const void* key) const override {
        return findNode(ValidatedKey(key)) != list_.end();
    }

    /**
     * @brief Adds an element with the specified key and value to the dictionary.
     *
     * C++ counterpart of .NET IDictionary.Add(object, object?).
     * @param key   Pointer used as the key (compared by address). Never null.
     * @param value Value to associate with the key.
     * @throws System::ArgumentNullException if @p key is null.
     * @throws System::ArgumentException     if the key already exists, matching .NET.
     *
     * @note **A duplicate key is rejected before any mutation and does not advance
     *       the counter**, so an outstanding enumerator stays valid across a failed
     *       Add. This is a deliberate deviation from .NET ListDictionaryInternal,
     *       which does `version++` first and unconditionally and therefore
     *       invalidates every enumerator on a call that changed nothing; .NET's own
     *       Hashtable does not, and this port follows the Hashtable rule --
     *       "advance on effective mutation" -- on both implementations of this
     *       interface (docs/ListDictionaryInternalSetterDesign.md sections 9.3, 15).
     * @note Add stays distinct from setItem: setItem replaces, Add rejects.
     */
    void Add(const void* key, void* value) override {
        const ValidatedKey k(key);
        if (findNode(k) != list_.end())
            throw System::ArgumentException("Item has already been added.");
        list_.push_back({k.getValueProperty(), value});
        ++version_;   // never reached by the throwing path: no partial mutation
    }

    /**
     * @brief Removes all elements from the dictionary.
     *
     * C++ counterpart of .NET IDictionary.Clear(), including its **unconditional**
     * version bump: clearing an already-empty dictionary still advances the counter
     * and still invalidates an outstanding enumerator. This is the one carve-out
     * from the "no bump for a no-op" rule, kept because .NET
     * ListDictionaryInternal.Clear() does exactly this and because it is the
     * existing behaviour of this port. (.NET Hashtable.Clear early-returns instead;
     * the two .NET implementations disagree on this row.)
     */
    void Clear() override { list_.clear(); ++version_; }

    /**
     * @brief Removes the element with the specified key from the dictionary.
     *
     * C++ counterpart of .NET IDictionary.Remove(object).
     * @param key Pointer used as the key (compared by address). Never null.
     * @throws System::ArgumentNullException if @p key is null.
     *
     * @note **Removing an absent key changes nothing and does not advance the
     *       counter**, so an outstanding enumerator stays valid. Second deliberate
     *       deviation from .NET ListDictionaryInternal's bump-first shape, on the
     *       same "advance on effective mutation" rule as Add above.
     * @note Erases the single located node rather than scanning with remove_if:
     *       Add rejects duplicates and setItem replaces, so at most one node can
     *       match, and unlinking one node is .NET's shape.
     */
    void Remove(const void* key) override {
        const auto it = findNode(ValidatedKey(key));
        if (it == list_.end()) return;   // absent: no mutation, no bump
        list_.erase(it);
        ++version_;
    }

    /**
     * @brief Returns an IDictionaryEnumerator for the dictionary.
     *
     * C++ counterpart of .NET IDictionary.GetEnumerator().
     * @return Heap-allocated IDictionaryEnumerator; caller takes ownership.
     */
    [[nodiscard]] IDictionaryEnumerator* GetEnumerator() override { return new NodeEnumerator(this); }

protected:
    /**
     * @brief Boxes every entry as std::any(DictionaryEntry) into the validated destination.
     *
     * Mirrors .NET's array.SetValue(new DictionaryEntry(...), index) into an
     * object[] destination; a caller retrieves each slot with
     * std::any_cast&lt;DictionaryEntry&gt;.
     * @param destination Destination validated by ICollection::CopyTo.
     * @param index       Validated zero-based destination index.
     */
    void copyToCore(ObjectSpan destination, intcs index) override {
        intcs i = index;
        for (const auto& n : list_) destination[i++] = std::any(DictionaryEntry(n.key, n.value));
    }
};

} // namespace System::Collections
