// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include "System/Collections/IEnumerator.hpp"
#include "System/Collections/DictionaryEntry.hpp"

namespace System::Collections {

/**
 * @brief Enumerates the elements of a non-generic dictionary.
 *
 * C++ counterpart of .NET System.Collections.IDictionaryEnumerator.
 *
 * @par Ownership and lifetime
 * All four accessors -- the inherited getCurrentProperty() and the three added
 * here -- return OWNING values. Nothing they return aliases the dictionary, and
 * nothing done to what they return can reach it. A returned value stays valid
 * after any later MoveNext(), Reset(), mutation of the dictionary, Clear(),
 * destruction of the enumerator, or destruction of the dictionary itself.
 *
 * @par The snapshot invariant
 * After MoveNext() returns true, an implementation must be able to answer every
 * accessor from state the enumerator itself owns. Reading container storage
 * inside an accessor is a defect even when the signature is right, because no
 * accessor performs the fail-fast version check that MoveNext() and Reset() do:
 * a mutation between MoveNext() and the accessor leaves the container iterator
 * invalid, and the accessor would dereference it with nothing to diagnose the
 * error. .NET's own HashtableEnumerator snapshots the key and the value into
 * enumerator fields at MoveNext() time and reads _buckets from no accessor.
 * Ticket #1794 made both implementations in this repository do the same;
 * docs/IDictionaryEnumeratorKeyValueSafetyDesign.md section 13 rule 4 records
 * the rule and section 8 the nine AddressSanitizer heap-use-after-free reports
 * that motivated it.
 *
 * @par Relationship between the accessors
 * getEntryProperty() is canonical. getKeyProperty() and getValueProperty()
 * return exactly that entry's Key and Value, and getCurrentProperty() returns
 * exactly that entry, boxed -- matching .NET's `public object Current => Entry;`.
 *
 * @par Deliberately absent: a version check on the accessors
 * The accessors do not fail fast when the dictionary was modified, and .NET's do
 * not either -- HashtableEnumerator.Key checks whether the enumerator is
 * positioned, not whether the version moved. The snapshot makes the resulting
 * stale read safe rather than turning a read .NET permits into an exception.
 */
class IDictionaryEnumerator : public IEnumerator {
public:
    /** @brief Virtual destructor for safe polymorphic destruction. */
    virtual ~IDictionaryEnumerator() = default;

    /**
     * @brief Gets both the key and the value of the current dictionary entry.
     *
     * C++ counterpart of .NET IDictionaryEnumerator.Entry, the unboxed form of
     * Current. This is the canonical representation: getKeyProperty() and
     * getValueProperty() return exactly this entry's Key and Value, and
     * getCurrentProperty() returns exactly this entry, boxed.
     *
     * @throws System::InvalidOperationException before the first MoveNext() or
     *         after enumeration has finished
     *         ("Enumeration has either not started or has already finished.").
     */
    [[nodiscard]] virtual DictionaryEntry getEntryProperty() const = 0;

    /**
     * @brief Gets the key of the current dictionary entry, as an owning box.
     *
     * C++ counterpart of .NET IDictionaryEnumerator.Key, which returns `object`.
     * Equal to getEntryProperty().getKeyProperty(). The returned std::any OWNS
     * its value: it is unaffected by any later MoveNext(), Reset(), mutation of
     * the dictionary, destruction of the enumerator, or destruction of the
     * dictionary, and writing to it cannot reach the dictionary.
     *
     * Recover the value with std::any_cast<T>(); query std::any::type() when the
     * boxed type is not known statically. The boxed type is implementation
     * defined and DIFFERS between implementations -- std::string for Hashtable,
     * const void* for ListDictionaryInternal -- because the two dictionaries
     * have genuinely different key domains (string-hashed versus
     * address-identity). A wrong std::any_cast throws std::bad_any_cast instead
     * of silently reinterpreting bytes, which the previous `const void*` return
     * did.
     *
     * If the boxed value is itself a handle -- ListDictionaryInternal's
     * `const void*`, or a std::shared_ptr<X> value in a Hashtable -- the box owns
     * the HANDLE and not the pointee, exactly as .NET returns the reference for a
     * reference type. Mutating the pointee is not mutating the dictionary and
     * moves no mutation counter; replacing the handle inside the returned box
     * cannot replace the dictionary's entry.
     *
     * @throws System::InvalidOperationException before the first MoveNext() or
     *         after enumeration has finished
     *         ("Enumeration has either not started or has already finished.").
     */
    [[nodiscard]] virtual std::any getKeyProperty() const = 0;

    /**
     * @brief Gets the value of the current dictionary entry, as an owning box.
     *
     * C++ counterpart of .NET IDictionaryEnumerator.Value, which returns
     * `object?`. Equal to getEntryProperty().getValueProperty(); an absent value
     * is an empty std::any (has_value() == false), matching .NET's null. Same
     * ownership, lifetime, casting, and handle-versus-pointee rules as
     * getKeyProperty().
     *
     * Where the dictionary's own element type is already std::any -- Hashtable's
     * mapped_type is -- the box holds the PAYLOAD, never a std::any nested inside
     * another std::any, because std::any(const std::any&) selects the copy
     * constructor rather than the value-forwarding one. One std::any_cast
     * recovers the stored value.
     *
     * @throws System::InvalidOperationException before the first MoveNext() or
     *         after enumeration has finished
     *         ("Enumeration has either not started or has already finished.").
     */
    [[nodiscard]] virtual std::any getValueProperty() const = 0;
};

} // namespace System::Collections
