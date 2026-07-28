// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <any>
#include "System/Collections/ICollection.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"

namespace System::Collections {

/**
 * @brief Represents a non-generic collection of key/value pairs.
 *
 * C++ counterpart of .NET System.Collections.IDictionary.
 * Because C++ has no unified object root, keys and values are typed as void*.
 */
class IDictionary : public ICollection {
public:
    /** @brief Virtual destructor for safe polymorphic destruction. */
    virtual ~IDictionary() = default;

    /**
     * @brief Gets the element with the specified key (getter).
     *
     * C++ counterpart of .NET IDictionary indexer getter (this[object key]), whose
     * getter is pure: it returns an independent handle to the value object, never a
     * handle to the dictionary's slot, and never mutates the dictionary.
     *
     * @param key The key of the element to get.
     * @return An **owning** std::any holding a copy of the stored value, or an empty
     *         std::any if the key is absent. Test with `has_value()`, or use
     *         Contains() when "absent" must be distinguished from "present but
     *         empty" -- .NET's own getter collapses the two into `null` for exactly
     *         the same reason, which is why it also has ContainsKey.
     *
     * @note Ticket #1796 changed this from `void*` to `std::any`. The previous
     *       signature returned a **writable pointer into live value storage out of a
     *       const member function**: a caller holding even a `const IDictionary&`
     *       could rewrite a stored value with the fail-fast mutation counter
     *       unmoved, and the pointer dangled after Remove/Clear/assignment/
     *       destruction (nine AddressSanitizer heap-use-after-free reports across
     *       fourteen scenarios). The returned copy is valid independently of the
     *       dictionary's later life. See docs/HashtableValueAccessSafetyDesign.md.
     * @warning The mangled name and vtable slot of this member are **unchanged**,
     *          but std::any is returned through a hidden sret pointer, so the
     *          calling convention is incompatible with the previous `void*`. A
     *          stale object file links with zero diagnostics and then crashes:
     *          every consumer must be fully rebuilt.
     */
    [[nodiscard]] virtual std::any getItem(const void* key) const = 0;

    /**
     * @brief Sets the element with the specified key (setter).
     *
     * C++ counterpart of .NET IDictionary indexer setter (this[object key] = value).
     * An implementation must advance its fail-fast mutation counter on both the
     * insert and the replace branch, matching .NET Hashtable.Insert.
     *
     * @param key   The key of the element to set.
     * @param value The value to associate with the key.
     *
     * @note The value parameter deliberately keeps its raw `void*` type (ticket
     *       #1796, design section 13.4). Migrating it to `const std::any&` is a
     *       measured data-corruption hazard: it makes the raw-address overload
     *       viable for `setItem("literal", v)`, the standard `const char*` ->
     *       `const void*` conversion then beats the user-defined `const char*` ->
     *       `std::string` one, and the entry silently lands under the stringified
     *       address of the literal with no diagnostic under
     *       -Wall -Wextra -Wpedantic -Werror. Prefer a typed overload where one
     *       exists (Hashtable::setItem(const std::string&, const std::any&)).
     */
    virtual void setItem(const void* key, void* value) = 0;

    /**
     * @brief Gets an ICollection containing the keys of the dictionary.
     *
     * C++ counterpart of .NET IDictionary.Keys. The returned collection is a live
     * view: it reflects later changes to the dictionary rather than snapshotting
     * the keys.
     *
     * @return A non-null, heap-allocated ICollection over the dictionary's keys;
     *         **the caller takes ownership** and must delete it, and it must not
     *         outlive the dictionary it views. .NET can return a cached view
     *         because the GC owns it; this port has no GC, so a returned reference
     *         type is caller-owned, matching GetEnumerator() throughout the port.
     *         An implementation must never return nullptr -- doing so turned every
     *         contract-following consumer into a null dereference (audit finding
     *         SR-AUD-363, ticket #1775).
     */
    [[nodiscard]] virtual ICollection* getKeysProperty() const = 0;

    /**
     * @brief Gets an ICollection containing the values of the dictionary.
     *
     * C++ counterpart of .NET IDictionary.Values; same liveness, ownership, and
     * non-null requirements as getKeysProperty().
     *
     * @return A non-null, heap-allocated ICollection over the dictionary's values;
     *         the caller takes ownership.
     */
    [[nodiscard]] virtual ICollection* getValuesProperty() const = 0;

    /**
     * @brief Returns true if the dictionary contains an element with the specified key.
     *
     * C++ counterpart of .NET IDictionary.Contains(object).
     * @param key The key to locate.
     */
    [[nodiscard]] virtual bool Contains(const void* key) const = 0;

    /**
     * @brief Adds an element with the given key and value to the dictionary.
     *
     * C++ counterpart of .NET IDictionary.Add(object, object?).
     * @param key   The key of the element to add.
     * @param value The value of the element to add.
     */
    virtual void Add(const void* key, void* value) = 0;

    /**
     * @brief Removes all elements from the dictionary.
     *
     * C++ counterpart of .NET IDictionary.Clear().
     */
    virtual void Clear() = 0;

    /**
     * @brief Removes the element with the specified key.
     *
     * C++ counterpart of .NET IDictionary.Remove(object).
     * @param key The key of the element to remove.
     */
    virtual void Remove(const void* key) = 0;

    /**
     * @brief Returns true if the dictionary is read-only.
     *
     * C++ counterpart of .NET IDictionary.IsReadOnly.
     */
    [[nodiscard]] virtual bool getIsReadOnlyProperty()  const { return false; }

    /**
     * @brief Returns true if the dictionary has a fixed size.
     *
     * C++ counterpart of .NET IDictionary.IsFixedSize.
     */
    [[nodiscard]] virtual bool getIsFixedSizeProperty() const { return false; }

    /**
     * @brief Returns an IDictionaryEnumerator for this dictionary.
     *
     * C++ counterpart of .NET IDictionary.GetEnumerator().
     * @return A heap-allocated IDictionaryEnumerator; caller takes ownership.
     */
    [[nodiscard]] virtual IDictionaryEnumerator* GetEnumerator() override = 0;
};

} // namespace System::Collections
