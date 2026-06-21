// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>

namespace System {

    /**
     * @brief A platform-specific type used to represent a pointer or a handle (unsigned).
     *
     * C++ counterpart of .NET System.UIntPtr.
     * Holds an unsigned integer large enough to hold a pointer on the target platform.
     */
    struct UIntPtr {
        /** @brief The underlying unsigned pointer-sized integer. */
        uintptr_t value = 0;

        /** @brief Default constructor — initialises value to 0. */
        UIntPtr() = default;
        UIntPtr(uintptr_t v) : value(v) {}       // NOLINT(google-explicit-constructor)
        UIntPtr(void* p) : value(reinterpret_cast<uintptr_t>(p)) {} // NOLINT(google-explicit-constructor)

        /** @brief The UIntPtr whose underlying value is zero. */
        static const UIntPtr Zero;

        /** @brief Returns true if both UIntPtr instances hold the same value. */
        [[nodiscard]] bool operator==(const UIntPtr& o) const { return value == o.value; }
        /** @brief Returns true if the two UIntPtr instances hold different values. */
        [[nodiscard]] bool operator!=(const UIntPtr& o) const { return value != o.value; }

        /** @brief Explicit conversion to the underlying uintptr_t integer. */
        [[nodiscard]] explicit operator uintptr_t() const { return value; }
        /** @brief Explicit conversion to a raw void pointer. */
        [[nodiscard]] explicit operator void*() const { return reinterpret_cast<void*>(value); }

        /** @brief Returns true if the underlying value is zero. */
        [[nodiscard]] bool IsZero() const { return value == 0; }
    };

    inline const UIntPtr UIntPtr::Zero{uintptr_t(0)};

} // namespace System
