// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <functional>
#include <optional>
#include <string>
#include "System/ResolveEventArgs.hpp"

namespace System {
    /**
     * @brief Represents a method that handles the event for resolving assemblies.
     *
     * C++ counterpart of the .NET `System.ResolveEventHandler` delegate,
     * `public delegate Assembly? ResolveEventHandler(object? sender, ResolveEventArgs args)`
     * (`ResolveEventHandler.cs:8`). The signature here is
     * `std::optional<std::string>(void* sender, ResolveEventArgs& args)`.
     *
     * @par The return became optional in ticket #2325
     * It used to be a plain `std::string`, which is a TOTAL function: a handler had no way to say
     * *"I could not resolve this"*. .NET's return is nullable and `null` means exactly that, and
     * the runtime then tries the next handler. **The empty string could not be borrowed for it**,
     * because empty already means something else in this API — `ResolveEventArgs` uses it for an
     * absent requesting assembly — so a documented sentinel would have been unenforceable by the
     * type, which is the defect itself rather than a repair for it.
     *
     * @note This alias is not yet wired to any assembly-resolution API; those remain stubs
     *       (SR-AUD-103). Fixing the signature now rather than after them is deliberate: the
     *       shape is decided by the reference, not by what will eventually call it, and a wrong
     *       signature in a shipped alias is harder to change once callers exist.
     */
    using ResolveEventHandler =
        std::function<std::optional<std::string>(void*, ResolveEventArgs&)>;
} // namespace System
