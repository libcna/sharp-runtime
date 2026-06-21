// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstdint>
#include <cstddef>

namespace System
{
    /**
     * @brief A platform-specific type used to represent a pointer or a handle.
     *
     * C++ counterpart of .NET System.IntPtr.
     * Holds a signed integer large enough to hold a pointer on the target platform.
     */
    struct IntPtr
    {
        /** @brief The underlying signed pointer-sized integer. */
        intptr_t value = 0;

        /** @brief Default constructor — initialises value to 0. */
        IntPtr() = default;

        IntPtr(intptr_t v) : value(v) {}           // NOLINT(google-explicit-constructor)
        IntPtr(void* p) : value(reinterpret_cast<intptr_t>(p)) {}  // NOLINT(google-explicit-constructor)

        /** @brief The IntPtr whose underlying value is zero. */
        static const IntPtr Zero;

        /** @brief Returns true if both IntPtr instances hold the same value. */
        [[nodiscard]] bool operator==(const IntPtr& other) const { return value == other.value; }
        /** @brief Returns true if the two IntPtr instances hold different values. */
        [[nodiscard]] bool operator!=(const IntPtr& other) const { return value != other.value; }

        /** @brief Explicit conversion to the underlying intptr_t integer. */
        [[nodiscard]] explicit operator intptr_t() const { return value; }
        /** @brief Explicit conversion to a raw void pointer. */
        [[nodiscard]] explicit operator void*() const { return reinterpret_cast<void*>(value); }

        /** @brief Returns true if the underlying value is zero. */
        [[nodiscard]] bool IsZero() const { return value == 0; }
    };

    inline const IntPtr IntPtr::Zero{intptr_t(0)};
}
