// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include "System/Collections/IEqualityComparer.hpp"

namespace System::Collections {

    /// Defines methods to support structural equality comparison of objects within a collection.
    class IStructuralEquatable {
    public:
        /// Destroys the equatable object.
        virtual ~IStructuralEquatable() = default;
        /// Determines structural equality with another object using the given comparer.
        [[nodiscard]] virtual bool Equals(const void* other, const IEqualityComparer& comparer) const = 0;
        /// Returns a structural hash code using the given comparer.
        [[nodiscard]] virtual std::size_t GetHashCode(const IEqualityComparer& comparer) const = 0;
    };

} // namespace System::Collections
