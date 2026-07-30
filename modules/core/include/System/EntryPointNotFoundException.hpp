// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <exception>
#include "System/TypeLoadException.hpp"

namespace System {

    /**
     * @brief The exception that is thrown when an attempt to load a class fails
     * because the entry point was not found.
     *
     * C++ counterpart of .NET System.EntryPointNotFoundException.
     */
    class EntryPointNotFoundException : public TypeLoadException {
    public:
        /** @brief Initializes a new instance with the default entry-point-not-found message. */
        EntryPointNotFoundException() : TypeLoadException("Entry point was not found.") {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131523)); // COR_E_ENTRYPOINTNOTFOUND
        }

        /** @brief Initializes a new instance with the specified message. */
        explicit EntryPointNotFoundException(const std::string& message) : TypeLoadException(message) {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131523)); // COR_E_ENTRYPOINTNOTFOUND
        }

        /**
         * @brief Initializes a new instance with a message and an inner exception.
         *
         * C++ counterpart of .NET EntryPointNotFoundException(string, Exception).
         */
        EntryPointNotFoundException(const std::string& message, std::exception_ptr inner)
            : TypeLoadException(message, std::move(inner)) {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131523)); // COR_E_ENTRYPOINTNOTFOUND
        }
    };

} // namespace System
