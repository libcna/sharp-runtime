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
#include "System/Collections/Generic/KeyNotFoundException.hpp"
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Collections/detail/MutationCounter.hpp"

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
     * begins with ArgumentNullException.ThrowIfNull(key) and then returns the value
     * object itself -- never a handle to the table's slot.
     *
     * @param key Raw key to look up.
     * @return An **owning copy** of the stored value, or an empty std::any if the key
     *         is absent. The copy is valid independently of this table: it survives
     *         Remove(), Clear(), copy assignment, move assignment and the table's
     *         destruction. "Absent" and "present but empty" both read as an empty
     *         std::any, exactly as .NET's getter returns `null` for both; use
     *         Contains()/ContainsKey() to tell them apart, or at() to throw.
     * @throws System::ArgumentNullException if @p key is null.
     *
     * @note This getter is pure: it never inserts and never advances the fail-fast
     *       mutation counter, so an outstanding enumerator stays valid across it.
     * @note Ticket #1796 changed the return type from `void*`. The old signature
     *       returned `const_cast<std::any*>(&it->second)` -- a writable pointer into
     *       live map storage handed out of a const member function -- through which a
     *       caller holding only a `const Hashtable&` or a `const IDictionary&` could
     *       rewrite a stored value with the counter unmoved, and which became a
     *       heap-use-after-free once the entry was erased, cleared, assigned over or
     *       destroyed.
     */
    [[nodiscard]] std::any getItem(const void* key) const override {
        return lookupCopy(toKey(key));
    }

    /**
     * @brief Sets the value associated with the specified key.
     *
     * C++ counterpart of .NET Hashtable indexer setter (this[object key] = value),
     * which reaches Insert and its ArgumentNullException.ThrowIfNull(key). Advances
     * the fail-fast mutation counter on the insert branch **and** on the replace
     * branch, including an equal-value replacement, because .NET's Insert calls
     * UpdateVersion() on both and never compares the old value.
     *
     * @param key   Raw key to set.
     * @param value Value to associate with @p key; null stores an empty std::any.
     * @throws System::ArgumentNullException if @p key is null.
     *
     * @note The value parameter deliberately keeps its raw `void*` type; see
     *       IDictionary::setItem and design section 13.4 for the measured
     *       data-corruption reason. setItem(const std::string&, const std::any&) is
     *       the typed, safe route.
     * @note The key is validated, and the value copied, before the map is touched, so
     *       a throwing write leaves both the contents and the counter unchanged.
     */
    void setItem(const void* key, void* value) override {
        std::string k = toKey(key);
        std::any copy = value ? *static_cast<std::any*>(value) : std::any{};
        _map[k] = std::move(copy);
        ++version_;
    }

    /**
     * @brief Typed, tracked setter for a string key -- the safe insert-or-replace route.
     *
     * Added by ticket #1796 as the canonical way to write a value without touching the
     * type-erased raw-key surface. Inserts when @p key is absent and replaces when it
     * is present, advancing the fail-fast mutation counter in both cases (and on an
     * equal-value replacement), matching .NET Hashtable.Insert.
     *
     * @param key   String key to set.
     * @param value Value to associate with @p key.
     *
     * @note The value is copied before the map is touched, so a throwing write leaves
     *       both the contents and the counter unchanged.
     */
    void setItem(const std::string& key, const std::any& value) {
        std::any copy = value;
        _map[key] = std::move(copy);
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
     *
     * @note Unlike the indexer setter, Add() rejects an existing key rather than
     *       replacing its value -- the .NET distinction between Add and
     *       `this[key] = value`, preserved exactly. A rejected or otherwise throwing
     *       Add leaves both the contents and the mutation counter unchanged.
     */
    void Add(const void* key, void* value) override {
        std::string k = toKey(key);
        if (_map.count(k)) throw System::ArgumentException("Item has already been added. Key in dictionary: '" + k + "'");
        std::any copy = value ? *static_cast<std::any*>(value) : std::any{};
        _map.emplace(std::move(k), std::move(copy));
        ++version_;
    }

    /**
     * @brief Adds a string key with a typed value.
     * @throws System::ArgumentException if the key already exists.
     *
     * @note Same Add-versus-indexer-setter distinction and same all-or-nothing
     *       guarantee as the raw-key overload above.
     */
    void Add(const std::string& key, const std::any& value) {
        if (_map.count(key)) throw System::ArgumentException("Item has already been added. Key in dictionary: '" + key + "'");
        std::any copy = value;
        _map.emplace(key, std::move(copy));
        ++version_;
    }

    /**
     * @brief Removes all key/value pairs from the table.
     *
     * C++ counterpart of .NET Hashtable.Clear(). Advances the fail-fast mutation
     * counter **unconditionally**, including on an already-empty table.
     *
     * @note This is a deliberate, decided deviation from .NET Hashtable, recorded
     *       by ticket #1802 rather than left implicit. .NET's Clear() early-returns
     *       without bumping when `_count == 0 && _occupancy == 0` (Hashtable.cs:426).
     *       `_occupancy` counts buckets whose collision bit was ever set, and
     *       std::unordered_map exposes no analogue, so the obvious `if (_map.empty())
     *       return;` would **not** reproduce .NET's rule -- it would skip the bump on
     *       an emptied-but-previously-colliding table where .NET still bumps. The
     *       port therefore keeps the unconditional bump, which errs in the safe
     *       direction: a spurious bump can only invalidate an enumerator that had
     *       nothing to read anyway, whereas a missed bump is the memory-unsafe error.
     *       .NET's own ListDictionaryInternal.Clear() also bumps unconditionally, so
     *       this matches one of .NET's two IDictionary implementations exactly.
     *       Remove() is the opposite case and **is** corrected: see below.
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
     *
     * @note Advances the fail-fast mutation counter **only when an entry was
     *       actually erased**; see removeKey() for the rule and its history.
     */
    void Remove(const void* key) override { removeKey(toKey(key)); }

    /**
     * @brief Removes the entry with the specified string key.
     * @param key String key to remove.
     *
     * @note Same effective-mutation versioning rule as the raw-key overload.
     */
    void Remove(const std::string& key) { removeKey(key); }

    /**
     * @brief Removes the entry with the specified C-string key.
     *
     * @param key C-string key to remove.
     * @throws System::ArgumentNullException if @p key is null. Without the check
     *         the argument reached std::string's null construction, which
     *         terminates with a std::logic_error that code catching
     *         System::Exception& cannot see (audit finding SR-AUD-363).
     *
     * @note Same effective-mutation versioning rule as the raw-key overload; the
     *       rejected null key throws before anything is erased or counted.
     */
    void Remove(const char* key) {
        if (key == nullptr) throw System::ArgumentNullException("key");
        removeKey(key);
    }

    /**
     * @brief The result of indexing a non-const Hashtable: an owning read, a tracked write.
     *
     * Returned by `operator[](const std::string&)` so that the C# spelling
     * `table[key] = value;` keeps compiling while becoming a *tracked* mutation. The
     * proxy stores the **key**, not a pointer to an element, so it is unaffected by
     * rehashing and remains meaningful even when the key is absent; and it never
     * hands out an alias into the table, so nothing a caller can hold survives into
     * freed storage.
     *
     * Ticket #1796 introduced it to close four defects that the previous
     * `std::any&` return had at once: an assignment through the reference did not
     * advance the fail-fast mutation counter; a bare *read* of an absent key
     * structurally inserted an entry (std::unordered_map::operator[]'s own rule),
     * changing Count while leaving outstanding enumerators apparently valid and
     * silently mis-enumerating -- measured at 4,008 entries as a walk over 2,045
     * distinct keys reaching only 6 of its 8 seed keys, throwing nothing and
     * producing no sanitizer report at all; and the reference dangled after
     * Remove()/Clear()/copy assignment/move assignment/destruction.
     *
     * @note A proxy borrows its table and must not outlive it, the same rule every
     *       enumerator and view in this port already carries. It is normally a
     *       temporary within one full-expression.
     */
    class ValueReference {
        Hashtable* owner_;
        std::string key_;

    public:
        /**
         * @brief Binds a proxy to a table and a key; the proxy borrows the table.
         * @param owner Table to read and write through; never null.
         * @param key   Key to read and write, copied into the proxy.
         */
        ValueReference(Hashtable* owner, std::string key)
            : owner_(owner), key_(std::move(key)) {}

        /**
         * @brief Deleted, and this is load-bearing rather than stylistic.
         *
         * std::any has a template converting constructor `any(T&&)` constrained only
         * on `is_copy_constructible_v<decay_t<T>>`. With a **copyable** proxy,
         * `std::any b = table[key];` prefers that constructor -- an identity
         * conversion of the argument -- over this class's own `operator std::any()`,
         * so `b` silently holds a ValueReference and the next std::any_cast throws
         * std::bad_any_cast **at run time with nothing wrong at compile time**.
         * Deleting the copy constructor removes the proxy from that constructor's
         * overload set, so the conversion operator is chosen and `b` holds the value.
         * Guaranteed copy elision still lets operator[] return the proxy by value.
         *
         * Deleting it also implicitly deletes the move constructor and the copy
         * assignment operator, so a proxy cannot be stored in a container, returned
         * from a function, or copied into a longer-lived variable.
         */
        ValueReference(const ValueReference&) = delete;

        /**
         * @brief Reads an owning copy of the value; an absent key yields an empty std::any.
         *
         * Deliberately returns **by value**, not `const std::any&`: a conversion
         * returning a reference trips GCC 14's -Wdangling-reference on the ordinary
         * `const std::any& r = table[key];` spelling, which every module here builds
         * with -Werror. The by-value conversion compiles every read spelling and
         * rejects exactly the four aliasing ones.
         *
         * Never inserts and never advances the mutation counter.
         */
        operator std::any() const { return owner_->lookupCopy(key_); }

        /** @brief Named form of the read conversion; returns an owning copy. */
        [[nodiscard]] std::any getValueProperty() const { return owner_->lookupCopy(key_); }

        /**
         * @brief Returns true if the key is present, distinguishing absent from empty.
         *
         * A read of an absent key and a read of a key stored with an empty std::any
         * both yield an empty std::any, matching .NET's getter returning `null` for
         * both. This is the discriminator, alongside ContainsKey().
         */
        [[nodiscard]] bool hasValueProperty() const { return owner_->ContainsKey(key_); }

        /**
         * @brief Inserts or replaces the value, advancing the mutation counter.
         *
         * Advances the counter on the insert branch, on the replace branch, **and on
         * an equal-value replacement**, because .NET's Hashtable.Insert calls
         * UpdateVersion() on both branches and never compares the old value. That is
         * deliberately the opposite of Generic::Dictionary's rule in this same
         * component, and both match their own .NET reference.
         *
         * The value is copied before the map is touched, so a throwing write leaves
         * both the contents and the counter unchanged.
         *
         * @param value Value to store under this proxy's key.
         * @return A reference to this proxy.
         */
        ValueReference& operator=(const std::any& value) {
            std::any copy = value;
            owner_->_map[key_] = std::move(copy);
            ++owner_->version_;
            return *this;
        }

        /**
         * @brief Writes another proxy's value THROUGH to this one's element, and bumps.
         *
         * `a[k1] = b[k2];` copies the value, exactly as it reads. This overload exists
         * because the copy assignment operator is implicitly deleted by the deleted
         * copy constructor, so without it a proxy-to-proxy assignment would not
         * compile at all.
         *
         * @param other Proxy to read the value from.
         * @return A reference to this proxy.
         */
        ValueReference& operator=(ValueReference&& other) {
            return *this = other.getValueProperty();
        }
    };

    /**
     * @brief Returns a tracked proxy for the given string key; a read never inserts.
     *
     * C++ counterpart of .NET's `Hashtable this[object key]` on a mutable table.
     * `table[key] = value;` performs one tracked insert-or-replace that advances the
     * fail-fast mutation counter; `std::any v = table[key];` reads an owning copy and
     * changes nothing -- neither Count, nor the counter, nor the validity of an
     * outstanding enumerator.
     *
     * @param key String key.
     * @return A ValueReference proxy; see that class for the ownership and lifetime
     *         contract, and for why it is deliberately non-copyable.
     *
     * @note Before ticket #1796 this returned `std::any&` straight out of
     *       `_map[key]`, so a bare read of an absent key inserted one and no write
     *       through the reference was tracked. `std::any& r = table[key];` and
     *       `&table[key]` no longer compile, by design;
     *       `const std::any& r = table[key];` still compiles and now binds a
     *       lifetime-extended **snapshot** rather than a live view.
     */
    [[nodiscard]] ValueReference operator[](const std::string& key) {
        return ValueReference(this, key);
    }

    /**
     * @brief Reads an owning copy of the value for the given string key on a const table.
     *
     * Added by ticket #1796 so that indexing a `const Hashtable&` is possible at all,
     * and so that it cannot possibly yield a write path. An absent key reads as an
     * empty std::any, matching .NET's getter and the non-const proxy's read; it never
     * inserts and never advances the mutation counter.
     *
     * @param key String key.
     * @return An owning copy of the stored value, or an empty std::any if absent.
     */
    [[nodiscard]] std::any operator[](const std::string& key) const { return lookupCopy(key); }

    /**
     * @brief Returns an owning copy of the value for the given string key, or throws.
     *
     * The throwing read, next to the empty-yielding reads on the indexer and
     * getItem().
     *
     * @param key String key.
     * @return An owning copy of the stored value, valid independently of this table.
     * @throws System::Collections::Generic::KeyNotFoundException if @p key is absent.
     *
     * @note Ticket #1796 changed both halves of this member. It returned
     *       `const std::any&` bound to live map storage, so
     *       `const_cast<std::any&>(table.at(k)) = v;` was well-formed, fully defined
     *       C++ that rewrote the dictionary with the counter unmoved, and the
     *       reference dangled once the table was cleared or destroyed; a by-value
     *       return is a prvalue, so that const_cast no longer compiles. And it threw
     *       `std::out_of_range`, which `catch (const System::Exception&)` cannot see,
     *       where it now throws the port's own KeyNotFoundException.
     */
    [[nodiscard]] std::any at(const std::string& key) const {
        auto it = _map.find(key);
        if (it == _map.end())
            throw System::Collections::Generic::KeyNotFoundException(
                "The given key '" + key + "' was not present in the dictionary.");
        return it->second;
    }

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
    /**
     * @brief The single lookup site behind every by-value read.
     *
     * getItem(), the const operator[] and ValueReference's read conversion all route
     * through this one function, so there is exactly one place where a Hashtable read
     * decides what an absent key means -- an empty std::any, matching .NET's getter
     * returning null. It is const, copies, and touches neither _map's contents nor
     * version_.
     *
     * @param key Already-converted string key.
     * @return An owning copy of the stored value, or an empty std::any if absent.
     */
    [[nodiscard]] std::any lookupCopy(const std::string& key) const {
        auto it = _map.find(key);
        return it == _map.end() ? std::any{} : it->second;
    }

    /**
     * @brief The single erase site behind all three Remove() overloads.
     *
     * All three route through here, so there is exactly one place where a
     * Hashtable removal decides whether it was an *effective* mutation -- the same
     * "decide once, structurally unskippable" shape lookupCopy() gives the reads
     * and toKey() gives the raw-key conversion.
     *
     * **The rule: advance the fail-fast mutation counter only when an entry was
     * actually erased.** std::unordered_map::erase(const key_type&) already
     * performs the lookup and returns how many elements it removed (0 or 1), so
     * the effective/no-op distinction costs no extra lookup, no extra key
     * conversion and no allocation -- the count that decides it is the value the
     * erase call already computed and previously discarded.
     *
     * This matches .NET Hashtable.Remove, which calls UpdateVersion() *inside* the
     * branch that found and cleared a bucket and leaves `_version` untouched when
     * the key is absent (Hashtable.cs:968-1005). It also matches the rule
     * `detail::MutationCounter` has always documented -- "bumped on each
     * **effective** structural mutation" -- and the rule
     * docs/ListDictionaryInternalSetterDesign.md section 9.3 selected for the whole
     * IDictionary interface, which ticket #1798 applied to the sibling
     * ListDictionaryInternal. With ticket #1802 both of this port's IDictionary
     * implementations agree on every version row.
     *
     * @note Before ticket #1802 each overload was `_map.erase(k); ++version_;` --
     *       the counter advanced whether or not the key was present, so a Remove
     *       that changed nothing threw InvalidOperationException out of every
     *       outstanding IDictionaryEnumerator, key-view enumerator and value-view
     *       enumerator. That is a **false positive**: memory-safe, but the exact
     *       opposite error from ticket #1798's, and the reason the two
     *       implementations disagreed on one of ten version rows.
     * @note The bump is after the erase, never before, so a throwing key
     *       conversion or a throwing erase leaves both the contents and the
     *       counter untouched -- a strong exception guarantee that .NET's own
     *       bump-first ListDictionaryInternal shape cannot offer.
     *
     * @param key Already-converted, already-validated string key.
     */
    void removeKey(const std::string& key) {
        if (_map.erase(key) != 0) ++version_;
    }

    std::unordered_map<std::string, std::any> _map;
    System::Collections::detail::MutationCounter version_;

    /**
     * Test-only seam (declared in detail/MutationCounter.hpp, never defined in
     * production) letting a regression position the mutation counter near a boundary.
     */
    friend struct SharpRuntime::Testing::CollectionVersionAccess<Hashtable>;

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
        System::Collections::detail::MutationVersion version_;
        std::unordered_map<std::string, std::any>::const_iterator it_;
        bool started_ = false;
        bool valid_ = false;
        // Not `mutable`: since ticket #1793 getCurrentProperty() returns a copy
        // rather than a pointer, so a const member function no longer needs a
        // non-const view of this cache.
        DictionaryEntry current_;

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

        /**
         * @brief Returns a copy of the current entry, boxed as std::any(DictionaryEntry).
         *
         * Recover it with std::any_cast<DictionaryEntry>. Modifying the copy
         * cannot reach the table, and no longer desynchronises the enumerator
         * from it either -- the pre-#1793 pointer to the `mutable` cache did both.
         */
        [[nodiscard]] std::any getCurrentProperty() const override {
            ensureCurrent();
            return std::any(current_);
        }

        [[nodiscard]] DictionaryEntry getEntryProperty() const override {
            ensureCurrent();
            return current_;
        }

        /**
         * @brief Returns a copy of the current entry's key, boxed as std::any(std::string).
         *
         * Answered from the MoveNext()-time snapshot, never from the map: before
         * ticket #1794 this returned `&it_->first`, an address into live
         * std::unordered_map storage that a caller could const_cast and rewrite,
         * leaving -- measured at 64 entries -- an entry Count still reported and
         * no lookup could return by either its old or its new key.
         */
        [[nodiscard]] std::any getKeyProperty() const override {
            ensureCurrent();
            return current_.getKeyProperty();
        }

        /**
         * @brief Returns a copy of the current entry's value, boxed.
         *
         * The box holds the stored value's own payload, **not** a std::any
         * wrapping the map's std::any. Answered from the MoveNext()-time
         * snapshot: before ticket #1794 this returned `&it_->second`, and because
         * the map's mapped_type is a NON-const std::any, const_cast plus
         * assignment through it was well-formed, fully defined C++ that rewrote
         * live dictionary storage with the mutation counter unmoved.
         */
        [[nodiscard]] std::any getValueProperty() const override {
            ensureCurrent();
            return current_.getValueProperty();
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
             * @brief Returns a copy of the current key or value, boxed.
             *
             * The key view boxes a copy of the key string, recovered with
             * std::any_cast<std::string>. The value view returns a copy of the
             * element's own std::any -- **not** a std::any wrapping a std::any --
             * so a caller recovers the stored value with one std::any_cast, the
             * same shape ArrayList::Enumerator uses.
             *
             * Before ticket #1793 this const_cast'd the inner accessor's
             * `const void*` and published a writable pointer into a live
             * std::unordered_map key, through which a caller could rewrite the
             * key in place and break the container's invariant. Ticket #1794
             * removed the type-erased address itself: the inner accessors now
             * return the owning box directly, so the static_cast pair this body
             * used to need is gone and the element shapes are unchanged.
             *
             * @throws System::InvalidOperationException before the first MoveNext,
             *         after the last element, or if the table was modified.
             */
            [[nodiscard]] std::any getCurrentProperty() const override {
                return keys_ ? inner_->getKeyProperty() : inner_->getValueProperty();
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
