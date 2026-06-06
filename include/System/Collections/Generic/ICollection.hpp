// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Collections/Generic/IEnumerable.hpp"

namespace System::Collections::Generic
{
    /**
     * @brief Defines methods to manipulate generic collections.
     *
     * C++ counterpart of the .NET System.Collections.Generic.ICollection<T> interface.
     *
     * @tparam T The type of elements in the collection.
     */
    template<typename T>
    class ICollection : public IEnumerable<T>
    {
    public:
        ~ICollection() override = default;

        /// Gets the number of elements in the collection.
        [[nodiscard]] virtual int getCountProperty() const = 0;

        /// Gets whether the collection is read-only.
        [[nodiscard]] virtual bool getIsReadOnlyProperty() const = 0;

        /// Adds an item to the collection.
        virtual void Add(const T& item) = 0;

        /// Removes all items from the collection.
        virtual void Clear() = 0;

        /// Determines whether the collection contains the specified item.
        [[nodiscard]] virtual bool Contains(const T& item) const = 0;

        /// Removes the first occurrence of the specified item.
        virtual bool Remove(const T& item) = 0;
    };
}
