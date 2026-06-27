// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <list>
#include <stdexcept>
#include "System/Collections/IDictionary.hpp"
#include "System/Collections/IDictionaryEnumerator.hpp"

namespace System::Collections {

/**
 * @brief Implements IDictionary using a singly linked list.
 *
 * C++ counterpart of .NET System.Collections.ListDictionaryInternal.
 * Recommended for collections that typically include fewer than 10 items.
 * Keys are compared by pointer identity.
 */
class ListDictionaryInternal : public IDictionary {
    struct Node {
        const void* key;
        void*       value;
    };
    std::list<Node> list_;

public:
    /** @brief Initializes a new empty ListDictionaryInternal. */
    ListDictionaryInternal() = default;

    /**
     * @brief Gets the number of key/value pairs contained in the dictionary.
     *
     * C++ counterpart of .NET ListDictionaryInternal.Count.
     */
    [[nodiscard]] int getCountProperty() const override {
        return static_cast<int>(list_.size());
    }

    /**
     * @brief Copies dictionary entries to a buffer starting at the given index (stub).
     *
     * C++ counterpart of .NET ICollection.CopyTo(Array, int).
     */
    void CopyTo(void* /*array*/, int /*index*/) override {}

    /**
     * @brief Gets the value associated with the specified key, or nullptr if not found.
     *
     * C++ counterpart of .NET ListDictionaryInternal indexer getter.
     * @param key Pointer used as the key (compared by address).
     */
    [[nodiscard]] void* getItem(const void* key) const override {
        for (const auto& n : list_) {
            if (n.key == key) return n.value;
        }
        return nullptr;
    }

    /**
     * @brief Sets the value for the key, adding a new entry if the key is not present.
     *
     * C++ counterpart of .NET ListDictionaryInternal indexer setter.
     * @param key   Pointer used as the key (compared by address).
     * @param value Value to associate with the key.
     */
    void setItem(const void* key, void* value) override {
        for (auto& n : list_) {
            if (n.key == key) { n.value = value; return; }
        }
        list_.push_back({key, value});
    }

    /**
     * @brief Gets the collection of keys (returns nullptr — stub).
     *
     * C++ counterpart of .NET IDictionary.Keys.
     */
    [[nodiscard]] ICollection* getKeysProperty() const override { return nullptr; }

    /**
     * @brief Gets the collection of values (returns nullptr — stub).
     *
     * C++ counterpart of .NET IDictionary.Values.
     */
    [[nodiscard]] ICollection* getValuesProperty() const override { return nullptr; }

    /**
     * @brief Determines whether the dictionary contains an element with the specified key.
     *
     * C++ counterpart of .NET IDictionary.Contains(object).
     * @param key Pointer used as the key (compared by address).
     */
    [[nodiscard]] bool Contains(const void* key) const override {
        for (const auto& n : list_) {
            if (n.key == key) return true;
        }
        return false;
    }

    /**
     * @brief Adds an element with the specified key and value to the dictionary.
     *
     * C++ counterpart of .NET IDictionary.Add(object, object?).
     * @throws std::invalid_argument if the key already exists.
     */
    void Add(const void* key, void* value) override {
        for (const auto& n : list_) {
            if (n.key == key) throw std::invalid_argument("Duplicate key");
        }
        list_.push_back({key, value});
    }

    /**
     * @brief Removes all elements from the dictionary.
     *
     * C++ counterpart of .NET IDictionary.Clear().
     */
    void Clear() override { list_.clear(); }

    /**
     * @brief Removes the element with the specified key from the dictionary.
     *
     * C++ counterpart of .NET IDictionary.Remove(object).
     * @param key Pointer used as the key (compared by address).
     */
    void Remove(const void* key) override {
        list_.remove_if([key](const Node& n){ return n.key == key; });
    }

    /**
     * @brief Returns an IDictionaryEnumerator for the dictionary (returns nullptr — stub).
     *
     * C++ counterpart of .NET IDictionary.GetEnumerator().
     */
    [[nodiscard]] IDictionaryEnumerator* GetEnumerator() override { return nullptr; }
};

} // namespace System::Collections
