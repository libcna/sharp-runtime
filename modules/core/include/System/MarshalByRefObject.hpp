// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    /**
     * @brief Enables access to objects across application domain boundaries in applications
     * that support remoting.
     *
     * C++ counterpart of .NET System.MarshalByRefObject.
     * In this port the class serves as a base-class marker; full remoting is not implemented.
     *
     * @warning **This class's public shape is not .NET's, in two ways that are
     * observable rather than cosmetic** (SR-AUD-128; the shape decision is
     * ticket #2297 and neither divergence is repaired by this note).
     *
     * - **It is directly constructible.** `System::MarshalByRefObject obj;`
     *   compiles here; .NET declares the class `abstract` with a `protected`
     *   constructor, so the C# equivalent is rejected outright (`CS0144`). Two
     *   in-repo tests construct the base today, so closing this is a public
     *   source break, not a tidy-up.
     * - **Three .NET members are absent**, not merely unimplemented:
     *   `GetLifetimeService()`, the virtual `InitializeLifetimeService()` and
     *   the protected `MemberwiseClone(bool)`. Current .NET keeps the first two
     *   specifically so that a caller receives `PlatformNotSupportedException`;
     *   removing them replaces that observable diagnostic with a compile error
     *   at an unrelated place. Adding them back is additive but introduces a
     *   virtual, which changes this class's vtable and that of every derived
     *   class — `AppDomain` and `ContextBoundObject` in this repository.
     *
     * What the port does provide is correct as far as it goes: the virtual
     * destructor makes polymorphic deletion through this base well defined,
     * which is what the derived types rely on.
     */
    class MarshalByRefObject {
    public:
        /** @brief Virtual destructor to allow safe polymorphic deletion. */
        virtual ~MarshalByRefObject() = default;
    };

} // namespace System
