// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Collections {

    /// Exposes a method that compares two non-generic objects.
    class IComparer {
    public:
        /// Destroys the comparer.
        virtual ~IComparer() = default;
        /// Compares two objects and returns an integer indicating their relative order.
        [[nodiscard]] virtual int Compare(const void* x, const void* y) const = 0;
    };

} // namespace System::Collections
