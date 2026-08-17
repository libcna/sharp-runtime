// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Exception.hpp"

namespace System::ComponentModel {

    using SharpRuntime::intcs;

    /** Represents an exception thrown by a Win32 API call, carrying the native error code. */
    class Win32Exception : public System::Exception {
        intcs nativeErrorCode_;
    public:
        /**
         * Constructs a Win32Exception from a numeric Windows error code.
         * @param errorCode The Win32 error code (e.g. from GetLastError()).
         */
        explicit Win32Exception(intcs errorCode)
            : System::Exception("Win32 error " + std::to_string(errorCode)),
              nativeErrorCode_(errorCode) {
            // Current .NET inherits E_FAIL from ExternalException. This reduced adapter has a
            // different public base, so assign the inherited reference value directly.
            setHResultProperty(static_cast<intcs>(0x80004005u)); // E_FAIL
        }

        /**
         * Constructs a Win32Exception with a custom message and error code.
         * @param errorCode The Win32 error code.
         * @param message   A human-readable description of the error.
         */
        Win32Exception(intcs errorCode, const std::string& message)
            : System::Exception(message), nativeErrorCode_(errorCode) {
            setHResultProperty(static_cast<intcs>(0x80004005u)); // E_FAIL
        }

        /**
         * @brief Constructs a Win32Exception with a message, an error code, and the exception
         *        that caused it.
         *
         * Ticket #2092 (SR-AUD-250). `WebSocketException`'s three-argument constructor contained
         * a literal `(void)innerException;` with the comment *"Win32Exception has no
         * inner-exception-carrying constructor to forward to"*. The comment was accurate, and
         * that is what blocked the ticket — but the conclusion drawn from it was not: the review
         * recorded the repair as *"a PUBLIC CONSTRUCTOR ADDITION ON A WIDELY DERIVED BASE and
         * possibly an object-layout change on every exception type in the repository"*.
         *
         * **It is neither.** `System::Exception` has carried `innerException_` and
         * `Exception(const std::string&, std::exception_ptr)` all along, so nothing gains a
         * member and no layout moves. And .NET's own `Win32Exception` has exactly this shape —
         * `public Win32Exception(string? message, Exception? innerException) : base(message,
         * innerException)` (`Win32Exception.cs:56`) — so the port was missing a constructor the
         * reference has, not inventing one.
         *
         * @note .NET's overload takes no error code because it defaults to
         *       `Marshal.GetLastPInvokeError()`, which has no meaning in this port. This
         *       overload therefore extends the existing `(errorCode, message)` form rather than
         *       reproducing .NET's signature exactly.
         *
         * @param errorCode The Win32 error code.
         * @param message   A human-readable description of the error.
         * @param inner     The exception that caused this one.
         */
        Win32Exception(intcs errorCode, const std::string& message, std::exception_ptr inner)
            : System::Exception(message, std::move(inner)), nativeErrorCode_(errorCode) {
            setHResultProperty(static_cast<intcs>(0x80004005u)); // E_FAIL
        }

        /** @return The underlying Win32 error code. */
        [[nodiscard]] intcs getNativeErrorCodeProperty() const { return nativeErrorCode_; }
    };

} // namespace System::ComponentModel
