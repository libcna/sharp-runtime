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
     * Partial C++ counterpart of .NET System.Collections.ObjectModel.KeyedCollection<TKey,TItem>.
     * Subclasses must implement GetKeyForItem().
     *
     * @note Status: Partial
     */
    template<typename TKey, typename TItem>
    class KeyedCollection : public Collection<TItem> {
        std::unordered_map<TKey, int> keyIndex_;
    protected:
        /** @brief Extracts the key from the given item. Must be implemented by subclass. */
        virtual TKey GetKeyForItem(const TItem& item) const = 0;

        /// Inserts item at index and rebuilds the key index.
        void InsertItem(int index, const TItem& item) override {
            TKey key = GetKeyForItem(item);
            keyIndex_[key] = index;
            Collection<TItem>::InsertItem(index, item);
            // rebuild index for items after insertion point
            rebuildIndex();
        }

        /// Removes the item at index and rebuilds the key index.
        void RemoveItem(int index) override {
            TKey key = GetKeyForItem(this->items_[index]);
            keyIndex_.erase(key);
            Collection<TItem>::RemoveItem(index);
            rebuildIndex();
        }

        /// Clears all items and the key index.
        void ClearItems() override {
            keyIndex_.clear();
            Collection<TItem>::ClearItems();
        }

    public:
        /// Default-constructs an empty KeyedCollection.
        KeyedCollection() = default;

        /// Returns true if the collection contains an item with the given key.
        [[nodiscard]] bool Contains(const TKey& key) const { return keyIndex_.count(key) > 0; }

        /// Removes the item with the given key; returns true if found and removed.
        bool Remove(const TKey& key) {
            auto it = keyIndex_.find(key);
            if (it == keyIndex_.end()) return false;
            this->RemoveAt(it->second);
            return true;
        }

        /// Returns a const reference to the item with the given key.
        [[nodiscard]] const TItem& operator[](const TKey& key) const {
            return this->items_.at(static_cast<size_t>(keyIndex_.at(key)));
        }

        /// Returns a reference to the item with the given key.
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
