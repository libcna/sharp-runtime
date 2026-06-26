// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Collections::Generic {

    /** Defines a method that a type implements to compare two objects of the same type. */
    template<typename T>
    class IComparer {
    public:
        /** Destroys the comparer. */
        virtual ~IComparer() = default;
        /** Compares two objects of type T and returns an integer indicating their relative order. */
        [[nodiscard]] virtual int Compare(const T& x, const T& y) const = 0;
    };

} // namespace System::Collections::Generic
