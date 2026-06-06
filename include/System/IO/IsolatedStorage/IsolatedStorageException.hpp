// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Exception.hpp"

namespace System::IO::IsolatedStorage
{
    /**
     * @brief Represents errors related to isolated storage operations.
     *
     * @note Status: IMPLEMENTED
     */
    class IsolatedStorageException : public System::Exception
    {
    public:
        /**
         * @brief Initializes a new instance of the IsolatedStorageException class.
         *
         * @param message Exception message.
         *
         * @note Status: IMPLEMENTED
         */
        explicit IsolatedStorageException(const std::string& message);
    };
}