// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Threading {

    /**
     * @brief The exception thrown when a failure occurs in a managed thread after the
     * underlying operating system thread has been started but before the thread is
     * ready to execute user code.
     *
     * C++ counterpart of .NET System.Threading.ThreadStartException.
     *
     * @note **The message is FIXED and there is no way to supply one.** .NET's type has exactly
     * two constructors and both pass `SR.Arg_ThreadStartException` -- *"Thread failed to
     * start."* -- to the base (`ThreadStartException.cs:13-24`). It has **no message-taking
     * constructor at all**, so `ThreadStartException("anything")` produced an exception .NET can
     * never produce while still claiming `COR_E_THREADSTART`. Ticket #1958 / SR-AUD-196 removed
     * the two message-taking constructors this port had invented; see
     * docs/Migration-ThreadStartExceptionShape.md.
     *
     * @note `final`, matching .NET's `public sealed class`.
     *
     * @note **One difference remains deliberately, and it is not an oversight**: .NET makes both
     * constructors `internal`, so no user can construct this type -- only the runtime does, in a
     * window that does not exist in this port (a failure *after* the OS thread starts but
     * *before* user code runs; here `std::thread` either constructs or throws
     * `std::system_error`). C++ has no `internal`, and the mechanical translations are not
     * equivalent.
     *
     * **Ticket #2390 settled this on 2026-08-19, and the answer is a general rule** --
     * `docs/StandingApprovals.md` **SA-12**, which applies to every ported type with `internal`
     * members. The rule is conditional on whether this port has a real creator: where it does,
     * the members become `private` and that creator is a `friend` (the shape #2298 used for
     * `LocalDataStoreSlot`); where it does not, the members **stay public** and the divergence is
     * recorded here. **This type is the second case**, because no code in this port can ever
     * throw it. Two alternatives were considered and declined: `private` with no friend makes the
     * type impossible to instantiate at all, leaving dead code and costing the tests that verify
     * the fixed message and `COR_E_THREADSTART` -- the only observable content it has; and
     * `private` plus a friend that never constructs it is a **dead friend declaration** that
     * looks faithful while granting access to a class that will never use it.
     *
     * **The divergence is in accessibility ALONE.** The parameter lists, the message, the HResult
     * and the `final` are .NET's exactly, so a caller who writes what .NET allows gets what .NET
     * gives.
     */
    class ThreadStartException final : public System::SystemException {
    public:
        /**
         * @brief Initializes a ThreadStartException.
         *
         * Counterpart of .NET's `internal ThreadStartException()`
         * (`ThreadStartException.cs:13-17`). The message is .NET's fixed
         * `SR.Arg_ThreadStartException`.
         */
        ThreadStartException()
            : SystemException("Thread failed to start.") {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131525u)); // COR_E_THREADSTART
        }
        /**
         * @brief Initializes a ThreadStartException with the exception that caused it.
         * @param reason The exception that caused this exception.
         *
         * Counterpart of .NET's `internal ThreadStartException(Exception? reason)`
         * (`ThreadStartException.cs:19-23`). Note the parameter is the **reason alone** -- the
         * message is still the fixed one, which is why this replaces the port's former
         * `(message, inner)` pair rather than sitting alongside it.
         */
        explicit ThreadStartException(std::exception_ptr reason)
            : SystemException("Thread failed to start.", std::move(reason)) {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131525u)); // COR_E_THREADSTART
        }
    };

} // namespace System::Threading
