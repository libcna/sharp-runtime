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
     * equivalent: `private` with no friend would make the type impossible to instantiate at all,
     * and a `friend` would have to name a class that never throws it. Choosing between those
     * changes the public surface, so it is **ticket #2390** rather than a guess made here.
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
