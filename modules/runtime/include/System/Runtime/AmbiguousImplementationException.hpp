// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include <string>
#include "System/Exception.hpp"

namespace System::Runtime {

    /**
     * @brief Thrown when the runtime cannot choose between two implementations.
     *
     * C++ counterpart of .NET `System.Runtime.AmbiguousImplementationException`
     * (`AmbiguousImplementationException.cs`), which is
     * `public sealed class AmbiguousImplementationException : Exception`.
     *
     * @note **#1980 G-3 reparented this from `SystemException` to `Exception` and sealed it, under
     * SA-15.3** — the approval that lifted SA-3's exclusion of vtable and base-class changes.
     *
     * @note **What that changes for a `catch`, enumerated rather than summarised**, which is
     * SA-15.3's fourth condition. The clause whose meaning moves is
     * `catch (const System::SystemException&)` around code that can throw this type: it used to
     * catch it and no longer does. **Measured across the repository and both consumers, there are
     * ZERO such clauses** — the 17 first-party `catch (SystemException)` sites are
     * exception-hierarchy tests for other types, and `cna`'s single one catches its own
     * `NoAudioHardwareException`; neither consumer names this type at all. So no existing handler
     * changes behaviour, and a future one is warned here. `catch (const System::Exception&)` and
     * `catch (const AmbiguousImplementationException&)` are unaffected.
     *
     * @note The third constructor is .NET's `(message, innerException)` overload
     * (`AmbiguousImplementationException.cs`), which SR-AUD-158 recorded as missing.
     */
    class AmbiguousImplementationException final : public System::Exception {
    public:
        AmbiguousImplementationException()
            : System::Exception("Ambiguous implementation found.") {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x8013106Au)); // COR_E_AMBIGUOUSIMPLEMENTATION
        }
        explicit AmbiguousImplementationException(const std::string& message)
            : System::Exception(message) {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x8013106Au));
        }
        AmbiguousImplementationException(const std::string& message,
                                          std::exception_ptr innerException)
            : System::Exception(message, innerException) {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x8013106Au));
        }
    };

} // namespace System::Runtime
