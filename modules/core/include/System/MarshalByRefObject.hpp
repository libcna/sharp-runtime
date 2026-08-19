// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/PlatformNotSupportedException.hpp"

namespace System {

    /**
     * @brief Enables access to objects across application domain boundaries in applications
     * that support remoting.
     *
     * C++ counterpart of .NET `System.MarshalByRefObject`
     * (`MarshalByRefObject.cs:10-33`), which is `public abstract` with a `protected`
     * constructor and whose two lifetime members exist only to throw
     * `PlatformNotSupportedException(SR.PlatformNotSupported_Remoting)`. Remoting is not
     * implemented in this port and never will be.
     *
     * @par What ticket #2297 changed
     * - **The constructor is `protected`**, matching .NET's, so `System::MarshalByRefObject obj;`
     *   no longer compiles — C# rejects the equivalent with `CS0144`. The copy and move members
     *   are protected with it, so the base cannot be reached by a slice either.
     * - **`GetLifetimeService()` is present and throws**, as .NET's does. Its absence turned an
     *   observable runtime diagnostic into a compile error at an unrelated place.
     *
     * @par What ticket #2374 added afterwards
     * - **`InitializeLifetimeService()` is present and `virtual`**, matching .NET
     *   (`MarshalByRefObject.cs:22-26`). #2297 left it absent because a virtual inserts a vtable
     *   **slot** in this class and in both derived ones, `AppDomain` and `ContextBoundObject`,
     *   which SA-3 excludes; the approval was granted per action on 2026-08-19 on the measurement
     *   that both downstream consumers have **zero** sites. It is `virtual` rather than plain
     *   because a non-virtual member of that name would silently break the one thing the .NET
     *   member is for: letting a derived type override the lease policy.
     *
     * @par What ticket #2297 deliberately did NOT change, and why
     * - **`MemberwiseClone(bool)` is still absent.** .NET's calls `Object.MemberwiseClone()`, and
     *   `System::Object` in this port declares no such member — measured. Adding it would be an
     *   invention rather than a port, which SA-5 forbids.
     * - **No `[[deprecated]]`.** Both .NET members carry `[Obsolete(RemotingApisMessage)]`;
     *   whether .NET's `Obsolete` becomes C++ `[[deprecated]]` anywhere in this repository is the
     *   undecided ticket #2289, and answering it incidentally here would settle it in the wrong
     *   place.
     *
     * The virtual destructor makes polymorphic deletion through this base well defined, which is
     * what the derived types rely on.
     */
    class MarshalByRefObject {
    protected:
        /**
         * @brief Protected default constructor, matching .NET's `protected MarshalByRefObject()`.
         *
         * Ticket #2297. The copy and move members are protected with it so the base cannot be
         * reached by a slice.
         */
        MarshalByRefObject() = default;
        MarshalByRefObject(const MarshalByRefObject&) = default;
        MarshalByRefObject(MarshalByRefObject&&) = default;
        MarshalByRefObject& operator=(const MarshalByRefObject&) = default;
        MarshalByRefObject& operator=(MarshalByRefObject&&) = default;

    public:
        /** @brief Virtual destructor to allow safe polymorphic deletion. */
        virtual ~MarshalByRefObject() = default;

        /**
         * @brief Always throws; remoting is not supported.
         *
         * C++ counterpart of .NET `MarshalByRefObject.GetLifetimeService()`
         * (`MarshalByRefObject.cs:17-21`), which throws
         * `PlatformNotSupportedException(SR.PlatformNotSupported_Remoting)`. It is **not**
         * `virtual` there, so adding it here costs no vtable slot.
         *
         * Added by ticket #2297: its absence turned an observable runtime diagnostic into a
         * compile error at an unrelated place.
         *
         * @return Never returns.
         * @throws System::PlatformNotSupportedException always.
         */
        [[noreturn]] void* GetLifetimeService() const {
            throw PlatformNotSupportedException("Remoting is not supported on this platform.");
        }

        /**
         * @brief Obtains a lifetime service object to control the lifetime policy for this
         * instance.
         *
         * C++ counterpart of .NET `MarshalByRefObject.InitializeLifetimeService()`
         * (`MarshalByRefObject.cs:22-26`), which throws
         * `PlatformNotSupportedException(SR.PlatformNotSupported_Remoting)`.
         *
         * @note **`virtual`, and that is the whole point of the member** — ticket **#2374**,
         * approved per action on 2026-08-19. .NET declares it `public virtual` so a derived type
         * can override the lease policy; a **non-virtual** member of the same name would compile,
         * look correct at every call site, and silently defeat the one thing the member exists
         * for. Ticket #2297 left it **absent** rather than add it non-virtually, for exactly that
         * reason, and pinned the absence with a detection idiom.
         *
         * The cost, accepted with the approval: this class already has a vtable (the destructor),
         * so adding a virtual inserts a **slot** here **and in both derived classes**,
         * `System::AppDomain` and `System::ContextBoundObject`. That is a silent binary break —
         * nothing fails to compile and every consumer must rebuild. Measured at the time of the
         * grant: **zero** `MarshalByRefObject`, `AppDomain` and `ContextBoundObject` sites in
         * `cna` and **zero** in `mobile-eggbert`.
         *
         * @note The return type is `void*` rather than `System::Object*`, matching the shape
         * `GetLifetimeService()` already uses here: .NET returns `object`, and this port's
         * `System::Object` is an abstract class that a lease object would have to derive from —
         * which is surface this port does not have and #2374 does not invent. Since the body
         * never returns, no caller can observe the difference.
         *
         * @return Never returns.
         * @throws System::PlatformNotSupportedException always.
         */
        [[noreturn]] virtual void* InitializeLifetimeService() {
            throw PlatformNotSupportedException("Remoting is not supported on this platform.");
        }
    };

} // namespace System
