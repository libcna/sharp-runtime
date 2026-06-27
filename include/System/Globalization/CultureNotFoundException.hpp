// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ArgumentException.hpp"

namespace System::Globalization {

/**
 * @brief The exception thrown when a culture identifier is not available on the current platform.
 *
 * C++ counterpart of .NET System.Globalization.CultureNotFoundException.
 * Extends ArgumentException to carry the invalid culture name or ID
 * that triggered the exception.
 */
class CultureNotFoundException : public System::ArgumentException {
    std::string invalidCultureName_;
    int invalidCultureId_ = -1;

public:
    /**
     * @brief Constructs a CultureNotFoundException with a default message.
     *
     * C++ counterpart of .NET CultureNotFoundException().
     */
    CultureNotFoundException() : ArgumentException("Culture is not supported.") {}

    /**
     * @brief Constructs a CultureNotFoundException with the specified message.
     *
     * C++ counterpart of .NET CultureNotFoundException(string).
     * @param message The error message.
     */
    explicit CultureNotFoundException(const std::string& message)
        : ArgumentException(message) {}

    /**
     * @brief Constructs a CultureNotFoundException with a message and invalid culture name.
     *
     * C++ counterpart of .NET CultureNotFoundException(string, string).
     * @param message             The error message.
     * @param invalidCultureName  The culture name that could not be found.
     */
    CultureNotFoundException(const std::string& message, const std::string& invalidCultureName)
        : ArgumentException(message, invalidCultureName),
          invalidCultureName_(invalidCultureName) {}

    /**
     * @brief Constructs a CultureNotFoundException with a message and inner exception.
     *
     * C++ counterpart of .NET CultureNotFoundException(string, Exception).
     * @param message The error message.
     * @param inner   The inner exception.
     */
    CultureNotFoundException(const std::string& message, std::exception_ptr inner)
        : ArgumentException(message, std::move(inner)) {}

    /**
     * @brief Constructs a CultureNotFoundException with a message and invalid LCID.
     *
     * C++ counterpart of .NET CultureNotFoundException(string, int, Exception).
     * @param message          The error message.
     * @param invalidCultureId The LCID that could not be found.
     * @param inner            The inner exception (may be nullptr).
     */
    CultureNotFoundException(const std::string& message, int invalidCultureId,
                             std::exception_ptr inner)
        : ArgumentException(message, std::move(inner)),
          invalidCultureId_(invalidCultureId) {}

    /**
     * @brief Constructs a CultureNotFoundException with a param name, invalid LCID, and message.
     *
     * C++ counterpart of .NET CultureNotFoundException(string, int, string).
     * @param paramName        The parameter name.
     * @param invalidCultureId The LCID that could not be found.
     * @param message          The error message.
     */
    CultureNotFoundException(const std::string& paramName, int invalidCultureId,
                             const std::string& message)
        : ArgumentException(message, paramName),
          invalidCultureId_(invalidCultureId) {}

    /**
     * @brief Gets the culture name that triggered this exception.
     *
     * C++ counterpart of .NET CultureNotFoundException.InvalidCultureName.
     * @return The invalid culture name, or an empty string if not set.
     */
    [[nodiscard]] const std::string& getInvalidCultureNameProperty() const {
        return invalidCultureName_;
    }

    /**
     * @brief Gets the LCID that triggered this exception.
     *
     * C++ counterpart of .NET CultureNotFoundException.InvalidCultureId.
     * @return The invalid culture ID, or -1 if not set.
     */
    [[nodiscard]] int getInvalidCultureIdProperty() const { return invalidCultureId_; }
};

} // namespace System::Globalization
