// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
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
     * @brief Gets or sets the element with the specified key (getter).
     *
     * C++ counterpart of .NET IDictionary indexer getter (this[object key]).
     * @param key The key of the element to get.
     * @return Pointer to the value, or nullptr if not found.
     */
    [[nodiscard]] virtual void* getItem(const void* key) const = 0;

    /**
     * @brief Sets the element with the specified key (setter).
     *
     * C++ counterpart of .NET IDictionary indexer setter (this[object key] = value).
     * @param key   The key of the element to set.
     * @param value The value to associate with the key.
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
