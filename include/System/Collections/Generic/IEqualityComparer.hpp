// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstddef>

namespace System::Collections::Generic {

    template<typename T>
    class IEqualityComparer {
    public:
        virtual ~IEqualityComparer() = default;
        [[nodiscard]] virtual bool Equals(const T& x, const T& y) const = 0;
        [[nodiscard]] virtual std::size_t GetHashCode(const T& obj) const = 0;
    };

} // namespace System::Collections::Generic
