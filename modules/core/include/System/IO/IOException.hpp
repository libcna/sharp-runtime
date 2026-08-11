// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <exception>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/SystemException.hpp"

namespace System::IO {

    /**
     * @brief The exception that is thrown when an I/O error occurs.
     *
     * @note Status: Implemented
     */
    class IOException : public System::SystemException {
    public:
        /** Initializes an IOException with a default message. */
        IOException();
        /** Initializes an IOException with the specified C-string message. */
        explicit IOException(const char* message);
        /** Initializes an IOException with the specified message. */
        explicit IOException(const std::string& message);
        /** Initializes an IOException with a message and an inner exception. */
        IOException(const std::string& message, std::exception_ptr inner);
        /**
         * @brief Initializes an IOException with a message and a caller-supplied HResult.
         *
         * C++ counterpart of .NET `IOException(string?, int)`. The supplied value replaces
         * the `COR_E_IO` (`0x80131620`) code every other constructor assigns, so an OS or
         * native error code can be preserved alongside a diagnostic message.
         *
         * @note Overload resolution. The literal `0` — and `NULL`, which GCC and Clang spell
         * as an integer null pointer constant — used to reach the inner-exception overload,
         * because a null pointer constant converts to `std::exception_ptr`. It now selects
         * this constructor, so `IOException(message, 0)` yields HResult `0` where it used to
         * yield `COR_E_IO`; the message and the absent inner exception are unchanged. Spell
         * the former meaning `IOException(message)`. `nullptr` is unaffected: it does not
         * convert to an integer, so it still selects the inner-exception overload.
         *
         * @param message The message that describes the error.
         * @param hresult An integer identifying the error that has occurred.
         */
        IOException(const std::string& message, SharpRuntime::intcs hresult);
    };

} // namespace System::IO
