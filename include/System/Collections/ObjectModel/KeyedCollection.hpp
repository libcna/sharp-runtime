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

        void InsertItem(int index, const TItem& item) override {
            TKey key = GetKeyForItem(item);
            keyIndex_[key] = index;
            Collection<TItem>::InsertItem(index, item);
            // rebuild index for items after insertion point
            rebuildIndex();
        }

        void RemoveItem(int index) override {
            TKey key = GetKeyForItem(this->items_[index]);
            keyIndex_.erase(key);
            Collection<TItem>::RemoveItem(index);
            rebuildIndex();
        }

        void ClearItems() override {
            keyIndex_.clear();
            Collection<TItem>::ClearItems();
        }

    public:
        KeyedCollection() = default;

        [[nodiscard]] bool Contains(const TKey& key) const { return keyIndex_.count(key) > 0; }

        bool Remove(const TKey& key) {
            auto it = keyIndex_.find(key);
            if (it == keyIndex_.end()) return false;
            this->RemoveAt(it->second);
            return true;
        }

        [[nodiscard]] const TItem& operator[](const TKey& key) const {
            return this->items_.at(static_cast<size_t>(keyIndex_.at(key)));
        }

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
