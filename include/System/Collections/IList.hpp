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
        virtual ~IList() = default;
        [[nodiscard]] virtual bool getIsReadOnlyProperty() const { return false; }
        [[nodiscard]] virtual bool getIsFixedSizeProperty() const { return false; }
        virtual void Add(void* value) = 0;
        virtual void Clear() = 0;
        virtual bool Contains(void* value) const = 0;
        virtual int IndexOf(void* value) const = 0;
        virtual void Insert(int index, void* value) = 0;
        virtual void Remove(void* value) = 0;
        virtual void RemoveAt(int index) = 0;
    };

} // namespace System::Collections
