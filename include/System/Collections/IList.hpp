// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/ICollection.hpp"

namespace System::Collections {

    /**
     * @brief Represents a non-generic collection of objects that can be individually accessed by index.
     *
     * Partial C++ counterpart of .NET System.Collections.IList.
     *
     * @note Status: Stub
     */
    class IList : public ICollection {
    public:
        /// Destroys the list.
        virtual ~IList() = default;
        /// Returns true if the list is read-only.
        [[nodiscard]] virtual bool getIsReadOnlyProperty() const { return false; }
        /// Returns true if the list is fixed-size.
        [[nodiscard]] virtual bool getIsFixedSizeProperty() const { return false; }
        /// Adds an item to the list.
        virtual void Add(void* value) = 0;
        /// Removes all items from the list.
        virtual void Clear() = 0;
        /// Returns true if the list contains the specified value.
        virtual bool Contains(void* value) const = 0;
        /// Returns the index of the first occurrence of a value, or -1 if not found.
        virtual int IndexOf(void* value) const = 0;
        /// Inserts an item at the specified index.
        virtual void Insert(int index, void* value) = 0;
        /// Removes the first occurrence of a value from the list.
        virtual void Remove(void* value) = 0;
        /// Removes the item at the specified index.
        virtual void RemoveAt(int index) = 0;
    };

} // namespace System::Collections
