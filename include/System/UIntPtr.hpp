// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>

namespace System {

    /**
     * @brief A platform-specific type used to represent a pointer or a handle (unsigned).
     *
     * Partial C++ counterpart of .NET System.UIntPtr.
     *
     * @note Status: Implemented
     */
    struct UIntPtr {
        uintptr_t value = 0;

        UIntPtr() = default;
        UIntPtr(uintptr_t v) : value(v) {}       // NOLINT(google-explicit-constructor)
        UIntPtr(void* p) : value(reinterpret_cast<uintptr_t>(p)) {} // NOLINT(google-explicit-constructor)

        static const UIntPtr Zero;

        [[nodiscard]] bool operator==(const UIntPtr& o) const { return value == o.value; }
        [[nodiscard]] bool operator!=(const UIntPtr& o) const { return value != o.value; }

        [[nodiscard]] explicit operator uintptr_t() const { return value; }
        [[nodiscard]] explicit operator void*() const { return reinterpret_cast<void*>(value); }

        [[nodiscard]] bool IsZero() const { return value == 0; }
    };

    inline const UIntPtr UIntPtr::Zero{uintptr_t(0)};

} // namespace System
