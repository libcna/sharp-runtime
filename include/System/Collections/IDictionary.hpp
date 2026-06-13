// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/ICollection.hpp"

namespace System::Collections {

    /**
     * @brief Represents a non-generic collection of key/value pairs.
     *
     * Partial C++ counterpart of .NET System.Collections.IDictionary.
     *
     * @note Status: Stub
     */
    class IDictionary : public ICollection {
    public:
        /// Destroys the dictionary.
        virtual ~IDictionary() = default;
        /// Returns true if the dictionary is read-only.
        [[nodiscard]] virtual bool getIsReadOnlyProperty()  const { return false; }
        /// Returns true if the dictionary is fixed-size.
        [[nodiscard]] virtual bool getIsFixedSizeProperty() const { return false; }
        /// Adds an element with the given key and value.
        virtual void Add(void* key, void* value) = 0;
        /// Removes all elements from the dictionary.
        virtual void Clear() = 0;
        /// Returns true if the dictionary contains an element with the given key.
        virtual bool Contains(void* key) const = 0;
        /// Removes the element with the given key.
        virtual void Remove(void* key) = 0;
    };

} // namespace System::Collections
