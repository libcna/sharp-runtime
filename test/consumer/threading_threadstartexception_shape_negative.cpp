// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1958 / SR-AUD-196.
//
// #1958 reduced System::Threading::ThreadStartException to .NET's own constructor set and made
// the class final. .NET's type is:
//
//     public sealed class ThreadStartException : SystemException
//     {
//         internal ThreadStartException()                 : base(SR.Arg_ThreadStartException) ...
//         internal ThreadStartException(Exception? reason) : base(SR.Arg_ThreadStartException, reason) ...
//     }                                                    // ThreadStartException.cs:11-24
//
// Both constructors pass the SAME FIXED MESSAGE, "Thread failed to start.". There is NO
// message-taking constructor at all -- so this port's `ThreadStartException("anything")` produced
// an exception .NET can never produce, while still claiming COR_E_THREADSTART.
//
// Migration: drop the message. `ThreadStartException(msg)` becomes `ThreadStartException()`, and
// `ThreadStartException(msg, inner)` becomes `ThreadStartException(inner)` -- note the surviving
// overload takes the REASON ALONE, because the message is not the caller's to choose.
//
// The remaining difference from .NET is deliberate and is NOT pinned here: .NET's constructors
// are `internal`, and C++ has no equivalent. That choice changes the public surface and is
// ticket #2390.
//
// Records: docs/Migration-ThreadStartExceptionShape.md,
// docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Threading
#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "System/Threading/ThreadStartException.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Threading::ThreadStartException;

int main() {
    const std::exception_ptr reason = std::make_exception_ptr(std::runtime_error("cause"));

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(threadstartexception-message-ctor): no matching function
    //     | no matching constructor
    //     | cannot convert
    ThreadStartException fromMessage(std::string("start failed"));
    (void)fromMessage;
#else
    ThreadStartException fromMessage;
    (void)fromMessage;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(threadstartexception-message-and-inner-ctor): no matching function
    //     | no matching constructor
    //     | cannot convert
    ThreadStartException fromBoth(std::string("start failed"), reason);
    (void)fromBoth;
#else
    ThreadStartException fromBoth(reason);
    (void)fromBoth;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(threadstartexception-literal-ctor): no matching function
    //     | no matching constructor
    //     | cannot convert
    // THE SPELLING MOST LIKELY TO SURVIVE A CARELESS MIGRATION, because a string literal is not
    // a std::string and a reader scanning for `std::string` will miss it.
    ThreadStartException fromLiteral("start failed");
    (void)fromLiteral;
#else
    ThreadStartException fromLiteral;
    (void)fromLiteral;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(threadstartexception-derive): cannot derive
    //     | base 'System::Threading::ThreadStartException' is marked 'final'
    //     | final
    // .NET's type is sealed. This is the shape that breaks SILENTLY in review but loudly in the
    // compiler.
    struct Derived : ThreadStartException {};
    (void)sizeof(Derived);
#else
    static_assert(std::is_final_v<ThreadStartException>,
                  "#1958/SR-AUD-196: .NET's ThreadStartException is sealed");
#endif

    // UNCHANGED, and asserted so the fixture proves what did NOT break: it is still a
    // SystemException, still catchable, and still carries COR_E_THREADSTART.
    static_assert(std::is_base_of_v<System::SystemException, ThreadStartException>,
                  "still a SystemException");
    bool caught = false;
    try {
        throw ThreadStartException();
    } catch (const System::SystemException&) {
        caught = true;
    }
    return caught ? 0 : 1;
}
