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
        virtual ~IDictionary() = default;

        [[nodiscard]] virtual int getCountProperty() const = 0;

        virtual TValue& operator[](const TKey& key) = 0;
        [[nodiscard]] virtual const TValue& operator[](const TKey& key) const = 0;

        [[nodiscard]] virtual bool ContainsKey(const TKey& key) const = 0;
        virtual void Add(const TKey& key, const TValue& value) = 0;
        virtual bool Remove(const TKey& key) = 0;
        virtual bool TryGetValue(const TKey& key, TValue& outValue) const = 0;

        virtual void Clear() = 0;
    };

} // namespace System::Collections::Generic
