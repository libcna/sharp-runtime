// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <memory>
#include "System/Collections/Generic/IEqualityComparer.hpp"

namespace System::Collections::Generic {

    // Compares objects by reference (pointer identity) rather than by value.
    template<typename T>
    class ReferenceEqualityComparer : public IEqualityComparer<T*> {
    public:
        [[nodiscard]] bool Equals(T* const& x, T* const& y) const override { return x == y; }
        [[nodiscard]] std::size_t GetHashCode(T* const& obj) const override {
            return std::hash<T*>{}(obj);
        }

        static ReferenceEqualityComparer& Instance() {
            static ReferenceEqualityComparer inst;
            return inst;
        }
    };

} // namespace System::Collections::Generic
