// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/IComparer.hpp"

namespace System::Collections {

    /// Supports structural comparison of objects within a collection.
    class IStructuralComparable {
    public:
        /// Destroys the comparable object.
        virtual ~IStructuralComparable() = default;
        /// Compares this object to another using the given comparer and returns an ordering integer.
        [[nodiscard]] virtual int CompareTo(const void* other, const IComparer& comparer) const = 0;
    };

} // namespace System::Collections
