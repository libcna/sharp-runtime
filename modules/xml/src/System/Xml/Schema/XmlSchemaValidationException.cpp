// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/Schema/XmlSchemaValidationException.hpp"

#include <utility>

namespace System::Xml::Schema {

    XmlSchemaValidationException::XmlSchemaValidationException() = default;

    XmlSchemaValidationException::XmlSchemaValidationException(const std::string& message)
        : XmlSchemaException(message) {}

    XmlSchemaValidationException::XmlSchemaValidationException(const std::string& message,
                                                                 std::exception_ptr innerException)
        : XmlSchemaException(message, std::move(innerException)) {}

    XmlSchemaValidationException::XmlSchemaValidationException(const std::string& message,
                                                                 std::exception_ptr innerException,
                                                                 SharpRuntime::intcs lineNumber,
                                                                 SharpRuntime::intcs linePosition)
        : XmlSchemaException(message, std::move(innerException), lineNumber, linePosition) {}

} // namespace System::Xml::Schema
