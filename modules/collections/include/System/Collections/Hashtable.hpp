// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "System/ArgumentException.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/InvalidOperationException.hpp"
#include "System/Collections/DictionaryEntry.hpp"
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System::Collections {

using SharpRuntime::bytecs;
using SharpRuntime::intcs;

namespace detail {

/**
 * @brief Best-effort std::any value equality, shared shape with ArrayList's
 * arrayListValueEquals() (see System/Collections/ArrayList.hpp for the full rationale):
 * std::any has no built-in equality operator, so common primitive/string types are compared
 * by value and anything else reports never-equal rather than a type-only false-positive match.
 */
inline bool hashtableValueEquals(const std::any& a, const std::any& b) {
    if (a.type() != b.type()) return false;
    if (const auto* p = std::any_cast<std::string>(&a))            return *p == *std::any_cast<std::string>(&b);
    if (const auto* p = std::any_cast<const char*>(&a))            return std::string(*p) == std::string(*std::any_cast<const char*>(&b));
    if (const auto* p = std::any_cast<bool>(&a))                   return *p == *std::any_cast<bool>(&b);
    if (const auto* p = std::any_cast<int>(&a))                    return *p == *std::any_cast<int>(&b);
    if (const auto* p = std::any_cast<long>(&a))                   return *p == *std::any_cast<long>(&b);
    if (const auto* p = std::any_cast<long long>(&a))              return *p == *std::any_cast<long long>(&b);
    if (const auto* p = std::any_cast<unsigned int>(&a))           return *p == *std::any_cast<unsigned int>(&b);
    if (const auto* p = std::any_cast<unsigned long>(&a))          return *p == *std::any_cast<unsigned long>(&b);
    if (const auto* p = std::any_cast<unsigned long long>(&a))     return *p == *std::any_cast<unsigned long long>(&b);
    if (const auto* p = std::any_cast<double>(&a))                 return *p == *std::any_cast<double>(&b);
    if (const auto* p = std::any_cast<float>(&a))                  return *p == *std::any_cast<float>(&b);
    if (const auto* p = std::any_cast<char>(&a))                   return *p == *std::any_cast<char>(&b);
    if (const auto* p = std::any_cast<void*>(&a))                  return *p == *std::any_cast<void*>(&b);
    return false;
}

} // namespace detail

/**
 * @brief Represents a collection of key/value pairs organized based on the hash code of the key.
 *
 * C++ counterpart of .NET System.Collections.Hashtable.
 * Keys are stored as std::string (address-stringified for void* keys, or direct for string overloads).
 * Values are stored as std::any.
 */
class Hashtable : public IDictionary {
public:
    /** @brief Constructs an empty Hashtable. */
    Hashtable() = default;
    /** @brief Constructs a Hashtable; the capacity hint is accepted but ignored in this port. */
    explicit Hashtable(int /*capacity*/) {}

    /**
     * @brief Returns the number of key/value pairs in the table.
     * C++ counterpart of .NET Hashtable.Count.
     */
    [[nodiscard]] intcs getCountProperty() const override {
        return static_cast<intcs>(_map.size());
    }

    /** @brief Keeps the inherited validating CopyTo overloads visible next to the typed one. */
    using ICollection::CopyTo;

    /**
     * @brief Copies each key/value pair into @p destination as a DictionaryEntry, starting at @p index.
     *
     * C++ counterpart of .NET Hashtable.CopyTo(Array, int), matching real .NET's default
     * behavior of copying DictionaryEntry elements (the typed KeyValuePair[] fast path doesn't
     * apply here, since C++ has no equivalent runtime array-element-type check). The
     * destination carries its own length, so the copy is bounds-checked before the first write.
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
        for (const auto& [k, v] : _map)
            destination[static_cast<size_t>(i++)] = DictionaryEntry(k, v);
    }

    /** @brief Returns false; Hashtable is never read-only. */
    [[nodiscard]] bool getIsReadOnlyProperty()  const override { return false; }
    /** @brief Returns false; Hashtable is never fixed-size. */
    [[nodiscard]] bool getIsFixedSizeProperty() const override { return false; }
    /** @brief Returns false; Hashtable is not thread-safe. */
    [[nodiscard]] bool getIsSynchronizedProperty() const override { return false; }

    /**
     * @brief Gets the value associated with the specified key.
     *
     * C++ counterpart of .NET Hashtable indexer getter (this[object key]), which
     * begins with ArgumentNullException.ThrowIfNull(key).
     * @param key Raw key to look up.
     * @return Pointer to the stored value, or nullptr if the key is absent.
     * @throws System::ArgumentNullException if @p key is null.
     */
    [[nodiscard]] void* getItem(const void* key) const override {
        auto k = toKey(key);
        auto it = _map.find(k);
        if (it == _map.end()) return nullptr;
        return const_cast<std::any*>(&it->second);
    }

    /**
     * @brief Sets the value associated with the specified key.
     *
     * C++ counterpart of .NET Hashtable indexer setter (this[object key] = value),
     * which reaches Insert and its ArgumentNullException.ThrowIfNull(key).
     * @param key   Raw key to set.
     * @param value Value to associate with @p key; null stores an empty std::any.
     * @throws System::ArgumentNullException if @p key is null.
     */
    void setItem(const void* key, void* value) override {
        _map[toKey(key)] = value ? *static_cast<std::any*>(value) : std::any{};
        ++version_;
    }

    /**
     * @brief Gets a live ICollection view over the keys of the Hashtable.
     *
     * C++ counterpart of .NET Hashtable.Keys, which returns a KeyCollection whose
     * Count, SyncRoot, IsSynchronized, GetEnumerator, and CopyTo all delegate to the
     * owning table, so "any changes to the hash table are reflected in this
     * collection". The view returned here has the same live behaviour; it holds no
     * copy of the keys.
     *
     * Each view element is the key boxed as std::any(std::string), matching how
     * DictionaryEntry stores a Hashtable key, so a CopyTo destination slot is read
     * back with std::any_cast&lt;std::string&gt;.
     *
     * @return A heap-allocated ICollection; **the caller takes ownership** and must
     *         delete it. That is the same convention as
     *         ListDictionaryInternal::getKeysProperty() and as GetEnumerator()
     *         throughout this port: .NET can hand back a cached view because the GC
     *         owns it, whereas this port has no GC, so a returned reference type is
     *         caller-owned. The view borrows the Hashtable and must not outlive it.
     * @note Before ticket #1775 this returned nullptr, so an IDictionary consumer
     *       using the documented view dereferenced null (audit finding SR-AUD-363).
     *       getKeys() remains available for a detached same-instant snapshot.
     */
    [[nodiscard]] ICollection* getKeysProperty() const override {
        return new MemberCollection(this, true);
    }

    /**
     * @brief Gets a live ICollection view over the values of the Hashtable.
     *
     * C++ counterpart of .NET Hashtable.Values; same live behaviour, ownership rule,
     * and SR-AUD-363 history as getKeysProperty(). Each view element is the stored
     * std::any value itself, so a CopyTo destination slot is read back with
     * std::any_cast of the value's own type. Use getValues() for a detached
     * same-instant snapshot.
     *
     * @return A heap-allocated ICollection; the caller takes ownership.
     */
    [[nodiscard]] ICollection* getValuesProperty() const override {
        return new MemberCollection(this, false);
    }

    /**
     * @brief Returns true if the Hashtable contains the specified key.
     *
     * C++ counterpart of .NET Hashtable.Contains(object), which forwards to
     * ContainsKey and its ArgumentNullException.ThrowIfNull(key).
     * @param key Raw key to locate.
     * @throws System::ArgumentNullException if @p key is null.
     */
    [[nodiscard]] bool Contains(const void* key) const override {
        return _map.count(toKey(key)) > 0;
    }

    /**
     * @brief Returns true if the table contains the specified string key.
     * @param key String key to look up.
     */
    bool ContainsKey(const std::string& key) const { return _map.count(key) > 0; }

    /**
     * @brief Returns true if any stored value has the same type as @p value.
     * C++ counterpart of .NET Hashtable.ContainsValue(object?).
     */
    bool ContainsValue(const std::any& value) const {
        for (const auto& [k, v] : _map)
            if (detail::hashtableValueEquals(v, value)) return true;
        return false;
    }

    /**
     * @brief Adds an element with the specified key and value.
     *
     * C++ counterpart of .NET Hashtable.Add(object, object?), which reaches Insert
     * and its ArgumentNullException.ThrowIfNull(key).
     * @param key   Raw key to add.
     * @param value Value to associate with @p key; null stores an empty std::any.
     * @throws System::ArgumentNullException if @p key is null.
     * @throws System::ArgumentException     if the key already exists.
     */
    void Add(const void* key, void* value) override {
        std::string k = toKey(key);
        if (_map.count(k)) throw System::ArgumentException("Item has already been added. Key in dictionary: '" + k + "'");
        _map[k] = value ? *static_cast<std::any*>(value) : std::any{};
        ++version_;
    }

    /**
     * @brief Adds a string key with a typed value.
     * @throws System::ArgumentException if the key already exists.
     */
    void Add(const std::string& key, const std::any& value) {
        if (_map.count(key)) throw System::ArgumentException("Item has already been added. Key in dictionary: '" + key + "'");
        _map[key] = value;
        ++version_;
    }

    /**
     * @brief Removes all key/value pairs from the table.
     * C++ counterpart of .NET Hashtable.Clear().
     */
    void Clear() override { _map.clear(); ++version_; }

    /**
     * @brief Removes the element with the specified key.
     *
     * C++ counterpart of .NET Hashtable.Remove(object), which begins with
     * ArgumentNullException.ThrowIfNull(key). Removing an absent key is not an
     * error, matching .NET.
     * @param key Raw key to remove.
     * @throws System::ArgumentNullException if @p key is null.
     */
    void Remove(const void* key) override { _map.erase(toKey(key)); ++version_; }

    /**
     * @brief Removes the entry with the specified string key.
     * @param key String key to remove.
     */
    void Remove(const std::string& key) { _map.erase(key); ++version_; }

    /**
     * @brief Removes the entry with the specified C-string key.
     *
     * @param key C-string key to remove.
     * @throws System::ArgumentNullException if @p key is null. Without the check
     *         the argument reached std::string's null construction, which
     *         terminates with a std::logic_error that code catching
     *         System::Exception& cannot see (audit finding SR-AUD-363).
     */
    void Remove(const char* key) {
        if (key == nullptr) throw System::ArgumentNullException("key");
        _map.erase(key);
        ++version_;
    }

    /**
     * @brief Returns a reference to the value for the given string key, inserting a default if absent.
     *
     * @note Unlike setItem()/the .NET indexer setter, an insertion or assignment made through
     * the reference this returns does not bump the fail-fast version counter (there is no way
     * to intercept a plain C++ reference assignment or std::unordered_map's own key-insertion) —
     * the same documented, narrow gap as ArrayList::operator[] (see ArrayList.hpp). Prefer
     * setItem()/Add() when fail-fast enumeration correctness matters.
     * @param key String key.
     */
    std::any& operator[](const std::string& key) { return _map[key]; }

    /**
     * @brief Returns a const reference to the value for the given string key.
     * @throws std::out_of_range if the key is absent.
     */
    const std::any& at(const std::string& key) const { return _map.at(key); }

    /** @brief Returns a vector of all string keys in the table. */
    std::vector<std::string> getKeys() const {
        std::vector<std::string> keys;
        keys.reserve(_map.size());
        for (const auto& [k, v] : _map) keys.push_back(k);
        return keys;
    }

    /** @brief Returns a vector of all values in the table. */
    std::vector<std::any> getValues() const {
        std::vector<std::any> vals;
        vals.reserve(_map.size());
        for (const auto& [k, v] : _map) vals.push_back(v);
        return vals;
    }

    /**
     * @brief Returns a heap-allocated enumerator over the Hashtable's entries; caller takes ownership.
     *
     * C++ counterpart of .NET Hashtable.GetEnumerator(). The enumerator throws
     * InvalidOperationException on MoveNext()/Reset()/getCurrentProperty() (or Key/Value/Entry access)
     * if the Hashtable is structurally modified while enumeration is in progress, matching
     * .NET's fail-fast contract. Iteration order is unspecified (std::unordered_map bucket
     * order), matching .NET's own documented "no particular order" guarantee for Hashtable.
     */
    IDictionaryEnumerator* GetEnumerator() override { return new Enumerator(this); }

protected:
    /**
     * @brief Boxes every key/value pair as std::any(DictionaryEntry) into the validated destination.
     *
     * Mirrors .NET's array.SetValue(new DictionaryEntry(...), i) into an object[]
     * destination; a caller retrieves each slot with std::any_cast&lt;DictionaryEntry&gt;.
     * @param destination Destination validated by ICollection::CopyTo.
     * @param index       Validated zero-based destination index.
     */
    void copyToCore(ObjectSpan destination, intcs index) override {
        intcs i = index;
        for (const auto& [k, v] : _map) destination[i++] = std::any(DictionaryEntry(k, v));
    }

private:
    std::unordered_map<std::string, std::any> _map;
    intcs version_ = 0;

    /**
     * @brief Single validating conversion site for every raw `const void*` key.
     *
     * Every raw-key entry point (getItem, setItem, Contains, Add, Remove) routes
     * through this one function, so a null key cannot reach the map through any
     * of them -- the same "validate once, structurally unskippable" shape that
     * ticket #1771 gave the copy boundary with
     * detail::requireValidCopyDestination. Matches .NET Hashtable, where
     * ContainsKey, Remove, and Insert (reached from Add and the indexer setter)
     * each begin with ArgumentNullException.ThrowIfNull(key).
     *
     * Rejecting null also removes an aliasing defect (audit finding
     * SR-AUD-363): a null key stringified to the address text "0", which is a
     * perfectly ordinary key for the Add(const std::string&, const std::any&)
     * overload, so a null raw key and the string key "0" silently named one
     * entry. No non-null address stringifies to "0", so the two key spaces can
     * no longer collide.
     *
     * @param key Raw key to convert.
     * @throws System::ArgumentNullException if @p key is null.
     */
    static std::string toKey(const void* key) {
        if (key == nullptr) throw System::ArgumentNullException("key");
        return std::to_string(reinterpret_cast<uintptr_t>(key));
    }

    /**
     * @brief Enumerator over a Hashtable's key/value pairs, matching the fail-fast versioning
     * pattern established by Queue::Enumerator/Stack::Enumerator/ArrayList::Enumerator in this
     * codebase.
     */
    class Enumerator : public IDictionaryEnumerator {
        const Hashtable* ht_;
        intcs version_;
        std::unordered_map<std::string, std::any>::const_iterator it_;
        bool started_ = false;
        bool valid_ = false;
        mutable DictionaryEntry current_;

    public:
        explicit Enumerator(const Hashtable* ht) : ht_(ht), version_(ht->version_), it_(ht->_map.begin()) {}

        bool MoveNext() override {
            if (version_ != ht_->version_) throw System::InvalidOperationException("Collection was modified; enumeration operation may not execute.");
            if (started_) { if (valid_) ++it_; } else { started_ = true; }
            valid_ = (it_ != ht_->_map.end());
            if (valid_) current_ = DictionaryEntry(it_->first, it_->second);
            return valid_;
        }

        void Reset() override {
            if (version_ != ht_->version_) throw System::InvalidOperationException("Collection was modified; enumeration operation may not execute.");
            it_ = ht_->_map.begin();
            started_ = false;
            valid_ = false;
        }

        [[nodiscard]] void* getCurrentProperty() const override {
            ensureCurrent();
            return &current_;
        }

        [[nodiscard]] DictionaryEntry getEntryProperty() const override {
            ensureCurrent();
            return current_;
        }

        [[nodiscard]] const void* getKeyProperty() const override {
            ensureCurrent();
            return &it_->first;
        }

        [[nodiscard]] const void* getValueProperty() const override {
            ensureCurrent();
            return &it_->second;
        }

    private:
        void ensureCurrent() const {
            if (!started_ || !valid_)
                throw System::InvalidOperationException("Enumeration has either not started or has already finished.");
        }
    };

    /**
     * @brief Live, read-only ICollection view over either the keys or the values.
     *
     * C++ counterpart of .NET Hashtable's private KeyCollection/ValueCollection,
     * merged into one class selected by a flag exactly as
     * ListDictionaryInternal::MemberCollection already does in this component.
     * Every member delegates to the owning Hashtable, so the view never holds a
     * copy and always reports the table's current contents.
     *
     * The view is private and only ever escapes as an ICollection*, so it needs no
     * typed CopyTo overload of its own: getCountProperty() plus the fixed std::any
     * element type is enough for a getKeysProperty()/getValuesProperty() consumer
     * to allocate a correct destination, and ICollection::CopyTo validates that
     * destination once before copyToCore runs (tickets #1771/#1774).
     */
    class MemberCollection : public ICollection {
        const Hashtable* table_;
        bool keys_; // true: view the keys; false: view the values

        /**
         * @brief Projects the owning table's IDictionaryEnumerator onto one member.
         *
         * Named MemberEnumerator rather than Enumerator so that it cannot be
         * confused with Hashtable::Enumerator, which it wraps and owns. The
         * projection keeps the table's fail-fast versioning: MoveNext()/Reset()
         * throw InvalidOperationException if the table was modified, and Current
         * throws before the first MoveNext and after the last element.
         */
        class MemberEnumerator : public IEnumerator {
            IDictionaryEnumerator* inner_;
            bool keys_;

        public:
            MemberEnumerator(IDictionaryEnumerator* inner, bool keys) : inner_(inner), keys_(keys) {}
            ~MemberEnumerator() override { delete inner_; }

            MemberEnumerator(const MemberEnumerator&) = delete;
            MemberEnumerator& operator=(const MemberEnumerator&) = delete;

            bool MoveNext() override { return inner_->MoveNext(); }
            void Reset() override { inner_->Reset(); }

            /**
             * @brief Returns the current key (as std::string*) or value (as std::any*).
             * @throws System::InvalidOperationException before the first MoveNext,
             *         after the last element, or if the table was modified.
             */
            [[nodiscard]] void* getCurrentProperty() const override {
                return const_cast<void*>(keys_ ? inner_->getKeyProperty()
                                               : inner_->getValueProperty());
            }
        };

    public:
        /**
         * @brief Binds a view to a table; the view borrows and must not outlive it.
         * @param table Owning Hashtable.
         * @param keys  true for the key view, false for the value view.
         */
        MemberCollection(const Hashtable* table, bool keys) : table_(table), keys_(keys) {}

        /** @brief Returns the owning table's current element count. */
        [[nodiscard]] intcs getCountProperty() const override { return table_->getCountProperty(); }

        /** @brief Mirrors the owning table's synchronization state, as .NET's views do. */
        [[nodiscard]] bool getIsSynchronizedProperty() const override {
            return table_->getIsSynchronizedProperty();
        }

        /** @brief Mirrors the owning table's SyncRoot, as .NET's views do. */
        [[nodiscard]] const void* getSyncRootProperty() const override {
            return table_->getSyncRootProperty();
        }

        /**
         * @brief Returns a heap-allocated enumerator over the viewed member; caller takes ownership.
         */
        [[nodiscard]] IEnumerator* GetEnumerator() override {
            return new MemberEnumerator(new Hashtable::Enumerator(table_), keys_);
        }

    protected:
        /**
         * @brief Boxes each key or value into the destination ICollection::CopyTo validated.
         *
         * Keys are boxed as std::any(std::string) and values are the stored
         * std::any itself, so both views hand a consumer the same element shape
         * the table's own DictionaryEntry copy uses.
         * @param destination Destination validated by ICollection::CopyTo.
         * @param index       Validated zero-based destination index.
         */
        void copyToCore(ObjectSpan destination, intcs index) override {
            intcs i = index;
            for (const auto& [k, v] : table_->_map)
                destination[i++] = keys_ ? std::any(k) : v;
        }
    };
};

} // namespace System::Collections
