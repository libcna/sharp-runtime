// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>

namespace System {

    /// @brief A platform-specific type used to represent a pointer or a handle (unsigned).
    ///
    /// Partial C++ counterpart of .NET System.UIntPtr.
    ///
    /// @note Status: Implemented
    struct UIntPtr {
        uintptr_t value = 0; ///< The underlying unsigned pointer-sized integer.

        /// Default constructor — initialises value to 0.
        UIntPtr() = default;
        UIntPtr(uintptr_t v) : value(v) {}       // NOLINT(google-explicit-constructor)
        UIntPtr(void* p) : value(reinterpret_cast<uintptr_t>(p)) {} // NOLINT(google-explicit-constructor)

        static const UIntPtr Zero; ///< The UIntPtr whose underlying value is zero.

        /// Returns true if both UIntPtr instances hold the same value.
        [[nodiscard]] bool operator==(const UIntPtr& o) const { return value == o.value; }
        /// Returns true if the two UIntPtr instances hold different values.
        [[nodiscard]] bool operator!=(const UIntPtr& o) const { return value != o.value; }

        /// Explicit conversion to the underlying uintptr_t integer.
        [[nodiscard]] explicit operator uintptr_t() const { return value; }
        /// Explicit conversion to a raw void pointer.
        [[nodiscard]] explicit operator void*() const { return reinterpret_cast<void*>(value); }

        /// Returns true if the underlying value is zero.
        [[nodiscard]] bool IsZero() const { return value == 0; }
    };

    inline const UIntPtr UIntPtr::Zero{uintptr_t(0)};

} // namespace System
