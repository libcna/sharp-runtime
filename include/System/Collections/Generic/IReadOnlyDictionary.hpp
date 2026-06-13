// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/Generic/IReadOnlyCollection.hpp"
#include "System/Collections/Generic/KeyValuePair.hpp"

namespace System::Collections::Generic {

    /**
     * @brief Represents a generic read-only collection of key/value pairs.
     *
     * Partial C++ counterpart of .NET System.Collections.Generic.IReadOnlyDictionary<TKey,TValue>.
     *
     * @note Status: Partial
     */
    template<typename TKey, typename TValue>
    class IReadOnlyDictionary : public IReadOnlyCollection<KeyValuePair<TKey,TValue>> {
    public:
        /// Destroys the dictionary.
        virtual ~IReadOnlyDictionary() = default;
        /// Returns a const reference to the value associated with the given key.
        [[nodiscard]] virtual const TValue& operator[](const TKey& key) const = 0;
        /// Returns true if the dictionary contains the specified key.
        [[nodiscard]] virtual bool ContainsKey(const TKey& key) const = 0;
        /// Gets the value associated with the specified key; returns true if found.
        [[nodiscard]] virtual bool TryGetValue(const TKey& key, TValue& value) const = 0;
    };

} // namespace System::Collections::Generic
