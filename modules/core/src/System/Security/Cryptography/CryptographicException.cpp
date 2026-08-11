// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Security/Cryptography/CryptographicException.hpp"

#include "System/String.hpp"

namespace System::Security::Cryptography {

    // Defined out of line so that the public header keeps its small include set:
    // System/String.hpp is needed only by this body, and every other constructor
    // of this type remains inline.
    CryptographicException::CryptographicException(const std::string& format, const std::string& insert)
        : System::SystemException(::System::String::Format(format, insert)) {}

} // namespace System::Security::Cryptography
