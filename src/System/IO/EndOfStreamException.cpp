// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/EndOfStreamException.hpp"

namespace System::IO {

    namespace {
        constexpr const char* DefaultMsg = "Attempted to read past the end of the stream.";
    }

    EndOfStreamException::EndOfStreamException()
        : IOException(DefaultMsg) {}

    EndOfStreamException::EndOfStreamException(const char* message)
        : IOException(message) {}

    EndOfStreamException::EndOfStreamException(const std::string& message)
        : IOException(message) {}

    EndOfStreamException::EndOfStreamException(const std::string& message, std::exception_ptr inner)
        : IOException(message, std::move(inner)) {}

} // namespace System::IO
