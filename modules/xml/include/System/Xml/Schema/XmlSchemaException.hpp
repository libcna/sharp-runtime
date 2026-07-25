// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include <exception>
#include <string>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/SystemException.hpp"

namespace System::Xml::Schema {

    class XmlSchemaObject;

    /**
     * @brief The exception thrown when an XML Schema error occurs.
     *
     * C++ counterpart of .NET System.Xml.Schema.XmlSchemaException. This type carries
     * schema-source location metadata even though sharp-runtime deliberately does not
     * implement an XSD validation engine. It is therefore available for ported code that
     * needs to report or catch a schema error without introducing a fake validator.
     */
    class XmlSchemaException : public System::SystemException {
        SharpRuntime::intcs lineNumber_ = 0;
        SharpRuntime::intcs linePosition_ = 0;
        std::string sourceUri_;
        XmlSchemaObject* sourceSchemaObject_ = nullptr;

    public:
        /** @brief Initializes a new instance with the default schema-error message. */
        XmlSchemaException();

        /** @brief Initializes a new instance with the specified error message. */
        explicit XmlSchemaException(const std::string& message);

        /** @brief Initializes a new instance with the specified message and inner exception. */
        XmlSchemaException(const std::string& message, std::exception_ptr innerException);

        /**
         * @brief Initializes a new instance with the specified message, inner exception, and source position.
         * @param message Error message.
         * @param innerException Exception that caused this error, or nullptr.
         * @param lineNumber Source line number, or zero if unavailable.
         * @param linePosition Source column number, or zero if unavailable.
         */
        XmlSchemaException(const std::string& message, std::exception_ptr innerException,
                           SharpRuntime::intcs lineNumber, SharpRuntime::intcs linePosition);

        /** @brief Gets the source line number, or zero if it is unavailable. */
        [[nodiscard]] SharpRuntime::intcs getLineNumberProperty() const noexcept { return lineNumber_; }

        /** @brief Gets the source column number, or zero if it is unavailable. */
        [[nodiscard]] SharpRuntime::intcs getLinePositionProperty() const noexcept { return linePosition_; }

        /** @brief Gets the URI of the schema source, or an empty string if it is unavailable. */
        [[nodiscard]] const std::string& getSourceUriProperty() const noexcept { return sourceUri_; }

        /**
         * @brief Gets the schema object associated with this error, or nullptr if unavailable.
         *
         * XmlSchemaObject itself belongs to the intentionally unported XSD object model, so
         * this practical exception-only port can only expose nullptr today. The pointer type
         * preserves source compatibility for consumers that only inspect the property.
         */
        [[nodiscard]] XmlSchemaObject* getSourceSchemaObjectProperty() const noexcept {
            return sourceSchemaObject_;
        }
    };

} // namespace System::Xml::Schema
