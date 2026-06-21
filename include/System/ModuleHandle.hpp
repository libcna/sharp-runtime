// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>

namespace System {

// Forward declaration — full definition in RuntimeTypeHandle.hpp (included by callers).
struct RuntimeTypeHandle;

/**
 * @brief Represents a runtime module.
 *
 * C++ counterpart of .NET System.ModuleHandle. In C++ there is no CLR module
 * metadata, so all methods are stubs that throw NotSupportedException or
 * return zero/empty values.
 */
struct ModuleHandle {
    /** @brief The empty module handle (all fields zero). */
    static const ModuleHandle EmptyHandle;

    /** @brief Gets the metadata stream version for this module. Always returns 0. */
    [[nodiscard]] int getMDStreamVersionProperty() const noexcept { return 0; }

    /** @brief Indicates whether this handle equals another. */
    [[nodiscard]] bool Equals(const ModuleHandle& other) const noexcept { return true; /* always EmptyHandle */ }

    /** @brief Returns a hash code for this handle. */
    [[nodiscard]] int GetHashCode() const noexcept { return 0; }

    bool operator==(const ModuleHandle& o) const noexcept { return Equals(o); }
    bool operator!=(const ModuleHandle& o) const noexcept { return !Equals(o); }

    // Token-resolution methods — all throw NotSupportedException.

    /** @brief Not supported — throws std::runtime_error. */
    [[noreturn]] RuntimeTypeHandle ResolveTypeHandle(int typeToken) const {
        throw std::runtime_error("ModuleHandle.ResolveTypeHandle is not supported.");
    }
};

inline const ModuleHandle ModuleHandle::EmptyHandle{};

} // namespace System
