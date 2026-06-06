// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>
#include "System/Collections/IEqualityComparer.hpp"

namespace System::Collections {

    class IStructuralEquatable {
    public:
        virtual ~IStructuralEquatable() = default;
        [[nodiscard]] virtual bool Equals(const void* other, const IEqualityComparer& comparer) const = 0;
        [[nodiscard]] virtual std::size_t GetHashCode(const IEqualityComparer& comparer) const = 0;
    };

} // namespace System::Collections
