// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>

namespace System::Collections {

    /// Defines methods to support comparison of non-generic objects for equality.
    class IEqualityComparer {
    public:
        /// Destroys the comparer.
        virtual ~IEqualityComparer() = default;
        /// Determines whether the two specified objects are equal.
        [[nodiscard]] virtual bool Equals(const void* x, const void* y) const = 0;
        /// Returns a hash code for the specified object.
        [[nodiscard]] virtual std::size_t GetHashCode(const void* obj) const = 0;
    };

} // namespace System::Collections
