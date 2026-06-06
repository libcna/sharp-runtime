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
        virtual ~IDictionary() = default;
        [[nodiscard]] virtual bool getIsReadOnlyProperty()  const { return false; }
        [[nodiscard]] virtual bool getIsFixedSizeProperty() const { return false; }
        virtual void Add(void* key, void* value) = 0;
        virtual void Clear() = 0;
        virtual bool Contains(void* key) const = 0;
        virtual void Remove(void* key) = 0;
    };

} // namespace System::Collections
