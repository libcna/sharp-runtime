// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/SystemException.hpp"

namespace System::Threading {

    /** The exception thrown when a synchronisation method is invoked from an unsynchronised block of code. */
    class SynchronizationLockException : public System::SystemException {
    public:
        /** Initializes a SynchronizationLockException with a default message. */
        SynchronizationLockException() : SystemException("Object synchronization method was called from an unsynchronized block of code.") {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131518u)); // COR_E_SYNCHRONIZATIONLOCK
        }
        /** Initializes a SynchronizationLockException with the specified message. */
        explicit SynchronizationLockException(const std::string& message) : SystemException(message) {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131518u)); // COR_E_SYNCHRONIZATIONLOCK
        }
        /** Initializes a SynchronizationLockException with a message and an inner exception. */
        SynchronizationLockException(const std::string& message, std::exception_ptr inner)
            : SystemException(message, std::move(inner)) {
            setHResultProperty(static_cast<SharpRuntime::intcs>(0x80131518u)); // COR_E_SYNCHRONIZATIONLOCK
        }
    };

} // namespace System::Threading
