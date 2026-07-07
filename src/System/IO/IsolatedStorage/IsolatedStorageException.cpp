// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/IsolatedStorage/IsolatedStorageException.hpp"
#include <utility>

namespace System::IO::IsolatedStorage
{
    namespace {
        constexpr const char* DefaultMessage = "An error occurred while accessing IsolatedStorage.";
    }

    IsolatedStorageException::IsolatedStorageException()
        : System::Exception(DefaultMessage)
    {
    }

    IsolatedStorageException::IsolatedStorageException(const std::string& message)
        : System::Exception(message)
    {
    }

    IsolatedStorageException::IsolatedStorageException(const std::string& message, std::exception_ptr inner)
        : System::Exception(message, std::move(inner))
    {
    }
}