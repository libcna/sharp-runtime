// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/IsolatedStorage/IsolatedStorageException.hpp"

namespace System::IO::IsolatedStorage
{
    IsolatedStorageException::IsolatedStorageException(const std::string& message)
        : System::Exception(message)
    {
    }
}