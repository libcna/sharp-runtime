// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Collections::Generic {

    template<typename T>
    class IComparer {
    public:
        virtual ~IComparer() = default;
        [[nodiscard]] virtual int Compare(const T& x, const T& y) const = 0;
    };

} // namespace System::Collections::Generic
