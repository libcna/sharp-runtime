// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/Generic/IReadOnlyCollection.hpp"

namespace System::Collections::Generic {

    template<typename T>
    class IReadOnlySet : public IReadOnlyCollection<T> {
    public:
        virtual ~IReadOnlySet() = default;
        [[nodiscard]] virtual bool Contains(const T& item) const = 0;
        [[nodiscard]] virtual bool IsSubsetOf(const IEnumerable<T>& other) const = 0;
        [[nodiscard]] virtual bool IsSupersetOf(const IEnumerable<T>& other) const = 0;
        [[nodiscard]] virtual bool IsProperSubsetOf(const IEnumerable<T>& other) const = 0;
        [[nodiscard]] virtual bool IsProperSupersetOf(const IEnumerable<T>& other) const = 0;
        [[nodiscard]] virtual bool Overlaps(const IEnumerable<T>& other) const = 0;
        [[nodiscard]] virtual bool SetEquals(const IEnumerable<T>& other) const = 0;
    };

} // namespace System::Collections::Generic
