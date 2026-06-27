// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <unordered_map>
#include "System/Collections/ObjectModel/Collection.hpp"

namespace System::Collections::ObjectModel {

/**
 * @brief Provides the abstract base class for a collection whose keys are embedded in the values.
 *
 * C++ counterpart of .NET System.Collections.ObjectModel.KeyedCollection<TKey,TItem>.
 * Subclasses must implement GetKeyForItem() to extract the key from each stored item.
 * Provides O(1) key-based lookup backed by an internal index.
 *
 * @tparam TKey  The type of keys in the collection.
 * @tparam TItem The type of items in the collection.
 */
template<typename TKey, typename TItem>
class KeyedCollection : public Collection<TItem> {
    std::unordered_map<TKey, int> keyIndex_;

protected:
    /**
     * @brief Extracts the key from the given item.
     *
     * C++ counterpart of .NET KeyedCollection<TKey,TItem>.GetKeyForItem(TItem).
     * Subclasses must implement this method to define the key for each item.
     * @param item The item from which to extract the key.
     * @return The key for the item.
     */
    virtual TKey GetKeyForItem(const TItem& item) const = 0;

    /**
     * @brief Inserts @p item at @p index and updates the internal key index.
     *
     * C++ counterpart of .NET KeyedCollection<TKey,TItem>.InsertItem(int, TItem).
     * @param index The zero-based index at which to insert.
     * @param item  The element to insert.
     */
    void InsertItem(int index, const TItem& item) override {
        Collection<TItem>::InsertItem(index, item);
        rebuildIndex();
    }

    /**
     * @brief Removes the item at @p index and updates the internal key index.
     *
     * C++ counterpart of .NET KeyedCollection<TKey,TItem>.RemoveItem(int).
     * @param index The zero-based index of the element to remove.
     */
    void RemoveItem(int index) override {
        Collection<TItem>::RemoveItem(index);
        rebuildIndex();
    }

    /**
     * @brief Clears all items and the internal key index.
     *
     * C++ counterpart of .NET KeyedCollection<TKey,TItem>.ClearItems().
     */
    void ClearItems() override {
        keyIndex_.clear();
        Collection<TItem>::ClearItems();
    }

public:
    /** @brief Default-constructs an empty KeyedCollection. */
    KeyedCollection() = default;

    /**
     * @brief Determines whether the collection contains an item with the specified key.
     *
     * C++ counterpart of .NET KeyedCollection<TKey,TItem>.Contains(TKey).
     * @param key The key to locate.
     * @return true if an item with the key is found; otherwise false.
     */
    [[nodiscard]] bool Contains(const TKey& key) const { return keyIndex_.count(key) > 0; }

    /**
     * @brief Gets the item with the specified key if it exists.
     *
     * C++ counterpart of .NET KeyedCollection<TKey,TItem>.TryGetValue(TKey, out TItem).
     * @param key  The key to locate.
     * @param item Receives the item if found.
     * @return true if the item was found; otherwise false.
     */
    bool TryGetValue(const TKey& key, TItem& item) const {
        auto it = keyIndex_.find(key);
        if (it == keyIndex_.end()) return false;
        item = this->items_[static_cast<size_t>(it->second)];
        return true;
    }

    /**
     * @brief Removes the item with the specified key from the collection.
     *
     * C++ counterpart of .NET KeyedCollection<TKey,TItem>.Remove(TKey).
     * @param key The key of the item to remove.
     * @return true if the item was found and removed; otherwise false.
     */
    bool Remove(const TKey& key) {
        auto it = keyIndex_.find(key);
        if (it == keyIndex_.end()) return false;
        this->RemoveAt(it->second);
        return true;
    }

    /**
     * @brief Returns a const reference to the item with the specified key.
     *
     * C++ counterpart of .NET KeyedCollection<TKey,TItem>.Item[TKey] getter.
     * @param key The key of the item to retrieve.
     * @return A const reference to the item.
     * @throws std::out_of_range if the key is not found.
     */
    [[nodiscard]] const TItem& operator[](const TKey& key) const {
        return this->items_.at(static_cast<size_t>(keyIndex_.at(key)));
    }

    /**
     * @brief Returns a reference to the item with the specified key.
     *
     * C++ counterpart of .NET KeyedCollection<TKey,TItem>.Item[TKey] setter.
     * @param key The key of the item to retrieve.
     * @return A reference to the item.
     * @throws std::out_of_range if the key is not found.
     */
    TItem& operator[](const TKey& key) {
        return this->items_.at(static_cast<size_t>(keyIndex_.at(key)));
    }

    using Collection<TItem>::operator[];

private:
    void rebuildIndex() {
        keyIndex_.clear();
        for (int i = 0; i < static_cast<int>(this->items_.size()); ++i)
            keyIndex_[GetKeyForItem(this->items_[i])] = i;
    }
};

} // namespace System::Collections::ObjectModel
