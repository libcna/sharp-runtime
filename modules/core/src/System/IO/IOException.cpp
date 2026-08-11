// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/IOException.hpp"

namespace System::IO {

    namespace {
        constexpr const char* DefaultMsg = "I/O error occurred.";
        constexpr SharpRuntime::intcs CorEIo = static_cast<SharpRuntime::intcs>(0x80131620);
    }

    IOException::IOException()
        : System::SystemException(DefaultMsg) {
        setHResultProperty(CorEIo);
    }

    IOException::IOException(const char* message)
        : System::SystemException(message) {
        setHResultProperty(CorEIo);
    }

    IOException::IOException(const std::string& message)
        : System::SystemException(message) {
        setHResultProperty(CorEIo);
    }

    IOException::IOException(const std::string& message, std::exception_ptr inner)
        : System::SystemException(message, std::move(inner)) {
        setHResultProperty(CorEIo);
    }

    IOException::IOException(const std::string& message, SharpRuntime::intcs hresult)
        : System::SystemException(message) {
        // .NET assigns the caller's code unconditionally rather than defaulting to
        // COR_E_IO first, so an explicit 0x80131620 and an explicit 0 are both honoured.
        setHResultProperty(hresult);
    }

} // namespace System::IO
