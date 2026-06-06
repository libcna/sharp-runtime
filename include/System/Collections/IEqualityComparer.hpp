// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>

namespace System::Collections {

    class IEqualityComparer {
    public:
        virtual ~IEqualityComparer() = default;
        [[nodiscard]] virtual bool Equals(const void* x, const void* y) const = 0;
        [[nodiscard]] virtual std::size_t GetHashCode(const void* obj) const = 0;
    };

} // namespace System::Collections
