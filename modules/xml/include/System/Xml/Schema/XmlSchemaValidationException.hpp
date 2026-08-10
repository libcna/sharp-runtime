// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <any>
#include <exception>
#include <string>
#include <utility>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Xml/Schema/XmlSchemaException.hpp"

namespace System::Xml::Schema {

    /**
     * @brief The exception thrown when XML Schema validation fails.
     *
     * C++ counterpart of .NET System.Xml.Schema.XmlSchemaValidationException. No XSD
     * validator is added by this type; it provides the public error contract so a ported
     * caller can construct, throw, and catch validation failures consistently.
     */
    class XmlSchemaValidationException : public XmlSchemaException {
        std::any sourceObject_;

    public:
        /** @brief Initializes a new instance with the default schema-error message. */
        XmlSchemaValidationException();

        /** @brief Initializes a new instance with the specified error message. */
        explicit XmlSchemaValidationException(const std::string& message);

        /** @brief Initializes a new instance with the specified message and inner exception. */
        XmlSchemaValidationException(const std::string& message, std::exception_ptr innerException);

        /**
         * @brief Initializes a new instance with the specified message, inner exception, and source position.
         * @param message Error message.
         * @param innerException Exception that caused this error, or nullptr.
         * @param lineNumber Source line number, or zero if unavailable.
         * @param linePosition Source column number, or zero if unavailable.
         */
        XmlSchemaValidationException(const std::string& message, std::exception_ptr innerException,
                                     SharpRuntime::intcs lineNumber, SharpRuntime::intcs linePosition);

        /**
         * @brief Gets the application object on which validation failed, or an empty value if unavailable.
         *
         * `std::any` is this runtime's mapping for a .NET `object` value.
         */
        [[nodiscard]] const std::any& getSourceObjectProperty() const noexcept { return sourceObject_; }

    protected:
        /**
         * @brief Associates an application object with this validation failure.
         *
         * C++ counterpart of .NET's protected-internal SetSourceObject(object).
         */
        void SetSourceObject(std::any sourceObject) noexcept { sourceObject_ = std::move(sourceObject); }
    };

} // namespace System::Xml::Schema
