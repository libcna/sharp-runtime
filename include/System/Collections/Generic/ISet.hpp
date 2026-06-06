// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Collections/Generic/ICollection.hpp"

namespace System::Collections::Generic {

    template<typename T>
    class ISet : public ICollection<T> {
    public:
        virtual ~ISet() = default;
        virtual bool Add(const T& item) = 0;
        virtual void UnionWith(const IEnumerable<T>& other) = 0;
        virtual void IntersectWith(const IEnumerable<T>& other) = 0;
        virtual void ExceptWith(const IEnumerable<T>& other) = 0;
        virtual void SymmetricExceptWith(const IEnumerable<T>& other) = 0;
        [[nodiscard]] virtual bool IsSubsetOf(const IEnumerable<T>& other) const = 0;
        [[nodiscard]] virtual bool IsSupersetOf(const IEnumerable<T>& other) const = 0;
        [[nodiscard]] virtual bool IsProperSubsetOf(const IEnumerable<T>& other) const = 0;
        [[nodiscard]] virtual bool IsProperSupersetOf(const IEnumerable<T>& other) const = 0;
        [[nodiscard]] virtual bool Overlaps(const IEnumerable<T>& other) const = 0;
        [[nodiscard]] virtual bool SetEquals(const IEnumerable<T>& other) const = 0;
    };

} // namespace System::Collections::Generic
