// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/NotSupportedException.hpp"

namespace System {

using SharpRuntime::intcs;

// Forward declaration — the definition arrives with the deferred include at the
// bottom of this header, which is what lets ModuleHandle and RuntimeTypeHandle
// refer to each other by value without either header having to be included first.
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
    [[nodiscard]] intcs getMDStreamVersionProperty() const noexcept { return 0; }

    /** @brief Indicates whether this handle equals another. */
    [[nodiscard]] bool Equals([[maybe_unused]] const ModuleHandle& other) const noexcept { return true; /* always EmptyHandle */ }

    /** @brief Returns a hash code for this handle. */
    [[nodiscard]] intcs GetHashCode() const noexcept { return 0; }

    bool operator==(const ModuleHandle& o) const noexcept { return Equals(o); }
    bool operator!=(const ModuleHandle& o) const noexcept { return !Equals(o); }

    // Token-resolution methods — all throw NotSupportedException.

    /**
     * @brief Not supported — throws NotSupportedException.
     *
     * Only declared here: a function *definition* needs a complete return type
     * ([dcl.fct]/12), while a declaration does not, so the body is deferred to
     * the bottom of this header.
     */
    [[noreturn]] RuntimeTypeHandle ResolveTypeHandle([[maybe_unused]] intcs typeToken) const;
};

inline const ModuleHandle ModuleHandle::EmptyHandle{};

} // namespace System

// Deferred: include RuntimeTypeHandle so that ResolveTypeHandle() can be defined
// with a complete return type. RuntimeTypeHandle.hpp defers its GetModuleHandle()
// body across the same cycle in the same way, so whichever of the two headers a
// translation unit names first, the other is fully defined before either body is
// compiled and both headers stand alone. Do not move this include above the
// definition of ModuleHandle, and do not move the body back into the class.
#include "System/RuntimeTypeHandle.hpp"

namespace System {

[[noreturn]] inline RuntimeTypeHandle
ModuleHandle::ResolveTypeHandle([[maybe_unused]] intcs typeToken) const {
    throw System::NotSupportedException("ModuleHandle.ResolveTypeHandle is not supported.");
}

} // namespace System
