// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/PlatformNotSupportedException.hpp"
#include "System/RuntimeTypeHandle.hpp"
#include "System/RuntimeArgumentHandle.hpp"
#include "System/TypedReference.hpp"

namespace System {

    /**
     * @brief Enumerates the arguments in a variable-length argument list.
     *
     * C++ counterpart of .NET System.ArgIterator.
     *
     * **Status: STUB, and .NET's own is one too.** `ArgIterator.cs:10-58` — the portable
     * implementation, which is the one a port must follow — throws
     * `PlatformNotSupportedException(SR.PlatformNotSupported_ArgIterator)` from **every single
     * member**, constructors included. The type depends on the CLR `__arglist` keyword, which
     * has no C++ counterpart, so there is nothing to implement on either side.
     *
     * @par Ticket #2276 settled the open question, and the reference answered it
     * The question was whether the instance members should become reachable, become `static`,
     * or stay as they are. **They stay instance members**, because .NET's are, and making them
     * `static` would diverge from the very shape this stub exists to present. What #2276 *did*
     * change is that they now behave like .NET's:
     *
     *  - `End()`, `Equals()` and `GetHashCode()` used to be `noexcept` and to return quietly
     *    (a no-op, `false`, and `0`). .NET throws from all three, so **a caller who reached one
     *    of them received a plausible answer where .NET reports an unsupported platform**;
     *  - the exception is `PlatformNotSupportedException`, not `NotSupportedException`;
     *  - `GetNextArgType()` returns `RuntimeTypeHandle`, as .NET's does, not `TypedReference`;
     *  - the `GetNextArg(RuntimeTypeHandle)` overload was missing and is added.
     *
     * @note Both constructors are `[[noreturn]]`, and declaring them suppresses the implicit
     * default constructor, so **no public construction can succeed** — exactly as in .NET, where
     * both constructors throw. The instance members are therefore unreachable through ordinary
     * use in both. A fixture that needs an instance has no legitimate route to one, and reaching
     * for raw storage instead is undefined behaviour rather than a workaround (SR-AUD-112).
     */
    struct ArgIterator {
        /**
         * @brief Constructs an ArgIterator over the given variable-argument handle.
         *
         * Always throws NotSupportedException; __arglist is not available in C++.
         * @param arglist Handle to the variable argument list.
         */
        [[noreturn]] explicit ArgIterator(RuntimeArgumentHandle /*arglist*/) {
            throw PlatformNotSupportedException("ArgIterator is not supported on this platform.");
        }

        /**
         * @brief Constructs an ArgIterator with an explicit pointer to the first argument.
         *
         * Always throws NotSupportedException.
         * @param arglist Handle to the variable argument list.
         * @param ptr     Pointer to the first argument descriptor.
         */
        [[noreturn]] ArgIterator(RuntimeArgumentHandle /*arglist*/, void* /*ptr*/) {
            throw PlatformNotSupportedException(
                "ArgIterator requires CLR __arglist support and is not available in sharp-runtime.");
        }

        /**
         * @brief Concludes processing of the argument list.
         *
         * Always throws, as .NET's does (`ArgIterator.cs:23-26`). It was a silent no-op until
         * ticket #2276.
         */
        [[noreturn]] void End() {
            throw PlatformNotSupportedException("ArgIterator is not supported on this platform.");
        }

        /**
         * @brief Always throws, as .NET's override does (`ArgIterator.cs:28-31`).
         *
         * It returned `false` until ticket #2276 — a plausible answer where .NET reports an
         * unsupported platform.
         */
        [[noreturn]] bool Equals(const ArgIterator& /*other*/) const {
            throw PlatformNotSupportedException("ArgIterator is not supported on this platform.");
        }

        /**
         * @brief Always throws, as .NET's override does (`ArgIterator.cs:33-36`).
         *
         * It returned `0` until ticket #2276.
         */
        [[noreturn]] int GetHashCode() const {
            throw PlatformNotSupportedException("ArgIterator is not supported on this platform.");
        }

        /**
         * @brief Returns the next argument in the variable-argument list.
         *
         * Always throws NotSupportedException.
         */
        [[noreturn]] TypedReference GetNextArg() {
            throw PlatformNotSupportedException("ArgIterator is not supported on this platform.");
        }

        /**
         * @brief Returns the next argument constrained to the specified runtime type.
         *
         * Always throws. **Added by ticket #2276** — .NET has this overload
         * (`ArgIterator.cs:44-48`) and this port did not.
         */
        [[noreturn]] TypedReference GetNextArg(RuntimeTypeHandle /*rth*/) {
            throw PlatformNotSupportedException("ArgIterator is not supported on this platform.");
        }

        /**
         * @brief Returns the runtime type of the next argument.
         *
         * Always throws. **The return type changed from `TypedReference` to
         * `RuntimeTypeHandle` in ticket #2276**, matching `ArgIterator.cs:50-53`.
         */
        [[noreturn]] RuntimeTypeHandle GetNextArgType() {
            throw PlatformNotSupportedException("ArgIterator is not supported on this platform.");
        }

        /**
         * @brief Returns the number of arguments remaining in the list.
         *
         * Always throws NotSupportedException.
         * @return Never returns.
         */
        [[noreturn]] int GetRemainingCount() {
            throw PlatformNotSupportedException("ArgIterator is not supported on this platform.");
        }
    };

} // namespace System
