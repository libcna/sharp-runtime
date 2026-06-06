// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/IComparer.hpp"

namespace System::Collections {

    class IStructuralComparable {
    public:
        virtual ~IStructuralComparable() = default;
        [[nodiscard]] virtual int CompareTo(const void* other, const IComparer& comparer) const = 0;
    };

} // namespace System::Collections
