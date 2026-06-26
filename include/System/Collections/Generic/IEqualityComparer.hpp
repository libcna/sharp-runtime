// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>

namespace System::Collections::Generic {

    /** Defines methods to support comparison of objects of type T for equality. */
    template<typename T>
    class IEqualityComparer {
    public:
        /** Destroys the comparer. */
        virtual ~IEqualityComparer() = default;
        /** Determines whether the two specified objects are equal. */
        [[nodiscard]] virtual bool Equals(const T& x, const T& y) const = 0;
        /** Returns a hash code for the specified object. */
        [[nodiscard]] virtual std::size_t GetHashCode(const T& obj) const = 0;
    };

} // namespace System::Collections::Generic
