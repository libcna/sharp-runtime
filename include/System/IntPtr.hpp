// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <cstdint>
#include <cstddef>

namespace System
{
    /// @brief A platform-specific type used to represent a pointer or a handle.
    ///
    /// C++ counterpart of the .NET System.IntPtr struct.
    /// Holds a signed integer large enough to hold a pointer on the target platform.
    struct IntPtr
    {
        intptr_t value = 0; ///< The underlying signed pointer-sized integer.

        /// Default constructor — initialises value to 0.
        IntPtr() = default;

        IntPtr(intptr_t v) : value(v) {}           // NOLINT(google-explicit-constructor)
        IntPtr(void* p) : value(reinterpret_cast<intptr_t>(p)) {}  // NOLINT(google-explicit-constructor)

        static const IntPtr Zero; ///< The IntPtr whose underlying value is zero.

        /// Returns true if both IntPtr instances hold the same value.
        [[nodiscard]] bool operator==(const IntPtr& other) const { return value == other.value; }
        /// Returns true if the two IntPtr instances hold different values.
        [[nodiscard]] bool operator!=(const IntPtr& other) const { return value != other.value; }

        /// Explicit conversion to the underlying intptr_t integer.
        [[nodiscard]] explicit operator intptr_t() const { return value; }
        /// Explicit conversion to a raw void pointer.
        [[nodiscard]] explicit operator void*() const { return reinterpret_cast<void*>(value); }

        /// Returns true if the underlying value is zero.
        [[nodiscard]] bool IsZero() const { return value == 0; }
    };

    inline const IntPtr IntPtr::Zero{intptr_t(0)};
}
