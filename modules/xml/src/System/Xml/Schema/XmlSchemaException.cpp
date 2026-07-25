// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Xml/Schema/XmlSchemaException.hpp"

#include <utility>

namespace System::Xml::Schema {

    namespace {
        constexpr const char* DefaultMessage = "A schema error occurred.";
        constexpr SharpRuntime::intcs HResultXmlSchema =
            static_cast<SharpRuntime::intcs>(0x80131941u); // HResults.XmlSchema
    }

    XmlSchemaException::XmlSchemaException()
        : SystemException(DefaultMessage) {
        setHResultProperty(HResultXmlSchema);
    }

    XmlSchemaException::XmlSchemaException(const std::string& message)
        : SystemException(message) {
        setHResultProperty(HResultXmlSchema);
    }

    XmlSchemaException::XmlSchemaException(const std::string& message, std::exception_ptr innerException)
        : SystemException(message, std::move(innerException)) {
        setHResultProperty(HResultXmlSchema);
    }

    XmlSchemaException::XmlSchemaException(const std::string& message, std::exception_ptr innerException,
                                             SharpRuntime::intcs lineNumber, SharpRuntime::intcs linePosition)
        : SystemException(message, std::move(innerException)),
          lineNumber_(lineNumber),
          linePosition_(linePosition) {
        setHResultProperty(HResultXmlSchema);
    }

} // namespace System::Xml::Schema
