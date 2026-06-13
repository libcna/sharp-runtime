// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Collections/Generic/ICollection.hpp"
#include "System/Collections/Generic/IEnumerable.hpp"

namespace System::Collections::Generic {

    /**
     * @brief Represents a generic collection of key/value pairs.
     *
     * @tparam TKey   The type of keys.
     * @tparam TValue The type of values.
     *
     * @note Status: Implemented (interface only)
     */
    template<typename TKey, typename TValue>
    class IDictionary : public IEnumerable<std::pair<TKey, TValue>> {
    public:
        /// Destroys the dictionary.
        virtual ~IDictionary() = default;

        /// Gets the number of key/value pairs in the dictionary.
        [[nodiscard]] virtual int getCountProperty() const = 0;

        /// Returns a reference to the value associated with the given key.
        virtual TValue& operator[](const TKey& key) = 0;
        /// Returns a const reference to the value associated with the given key.
        [[nodiscard]] virtual const TValue& operator[](const TKey& key) const = 0;

        /// Returns true if the dictionary contains the specified key.
        [[nodiscard]] virtual bool ContainsKey(const TKey& key) const = 0;
        /// Adds the specified key and value to the dictionary.
        virtual void Add(const TKey& key, const TValue& value) = 0;
        /// Removes the value with the specified key and returns true if successful.
        virtual bool Remove(const TKey& key) = 0;
        /// Gets the value associated with the specified key; returns true if found.
        virtual bool TryGetValue(const TKey& key, TValue& outValue) const = 0;

        /// Removes all key/value pairs from the dictionary.
        virtual void Clear() = 0;
    };

} // namespace System::Collections::Generic
