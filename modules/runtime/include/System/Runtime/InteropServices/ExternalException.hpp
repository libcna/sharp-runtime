// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/SystemException.hpp"
#include <cstdio>
#include <string>

namespace System::Runtime::InteropServices
{
    /**
     * @brief Base class for exceptions thrown by, or related to, external (unmanaged) code.
     *
     * @note <b>#1980 group G-1 (SR-AUD-159).</b> The `(message, errorCode)` constructor,
     * `getErrorCodeProperty()` and `ToString()` were absent. `ErrorCode` needed <b>no data
     * member</b>: .NET's is `public virtual int ErrorCode => HResult;` (`ExternalException.cs`),
     * an alias for the HResult this type already sets, so `sizeof` is unchanged and no consumer
     * rebuilds. The finding's implication that it is separate state does not survive the
     * reference.
     */
    class ExternalException : public System::SystemException
    {
    public:
        ExternalException()
            : System::SystemException("External component has thrown an exception.") {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x80004005u)); // E_FAIL
        }

        explicit ExternalException(const std::string& message)
            : System::SystemException(message) {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x80004005u)); // E_FAIL
        }

        ExternalException(const std::string& message, std::exception_ptr inner)
            : System::SystemException(message, std::move(inner)) {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x80004005u)); // E_FAIL
        }

        /**
         * @brief Creates an exception carrying a specific HRESULT.
         *
         * `ExternalException.cs`: `public ExternalException(string? message, int errorCode)`
         * sets `HResult = errorCode` -- it does NOT default to E_FAIL, which is the whole point
         * of the overload.
         */
        ExternalException(const std::string& message, SharpRuntime::intcs errorCode)
            : System::SystemException(message) {
            setHResultProperty(errorCode);
        }

        /**
         * @brief The HRESULT of the error.
         *
         * .NET's is `public virtual int ErrorCode => HResult;` -- an alias, not a second field.
         * It is non-virtual here because making it virtual would add a slot to this class's
         * vtable, which `docs/StandingApprovals.md` SA-3 excludes; nothing in this repository
         * derives from `ExternalException` (measured -- `Win32Exception` derives from
         * `System::Exception` directly and says so at its own site), so no override is lost.
         */
        [[nodiscard]] SharpRuntime::intcs getErrorCodeProperty() const {
            return getHResultProperty();
        }

        /**
         * @note <b>`ToString()` is deliberately ABSENT, and #1980 measured why rather than
         * assuming it.</b> .NET's is `$"{GetType()} (0x{HResult:X8})"` plus the message and any
         * inner exception -- and `GetType()` is the MOST DERIVED type, which is reflection this
         * port permanently lacks.
         *
         * It was implemented, with the type name resolved statically at this site as #2323 did
         * for `Exception`'s fallback message, and then <b>removed on the downstream
         * measurement</b> SA-2 condition 5 requires: `cna` derives from this class in
         * <b>three</b> types -- `NoAudioHardwareException`, `InstancePlayLimitException` and
         * `StorageDeviceNotConnectedException` -- every one of which the static name would have
         * misnamed. That is exactly #2323's own rule, in its own words: <i>a message naming the
         * wrong type is a lie, where an empty one is merely an absence.</i>
         *
         * The remaining routes both cost more than this ticket may spend: a virtual `ToString()`
         * adds a slot to this class's vtable, which SA-3 excludes, and a stored type name is a
         * data member on a class three downstream types derive from. Ticket <b>#2387</b>.
         */
        ~ExternalException() override = default;
    };

} // namespace System::Runtime::InteropServices
