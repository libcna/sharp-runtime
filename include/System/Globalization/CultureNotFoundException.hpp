// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ArgumentException.hpp"

namespace System::Globalization {

/// <summary>Thrown when a culture identifier is not available on the current platform.</summary>
class CultureNotFoundException : public System::ArgumentException {
    std::string invalidCultureName_;
public:
    /// Constructs with a default "Culture is not supported." message.
    CultureNotFoundException() : ArgumentException("Culture is not supported.") {}
    /// Constructs with a custom @p message.
    explicit CultureNotFoundException(const std::string& message) : ArgumentException(message) {}
    /// Constructs with @p message and the @p invalidCultureName that caused the exception.
    CultureNotFoundException(const std::string& message, const std::string& invalidCultureName)
        : ArgumentException(message, invalidCultureName), invalidCultureName_(invalidCultureName) {}
    /// Constructs with @p message and wraps an @p inner exception's what() text.
    CultureNotFoundException(const std::string& message, const std::exception& inner)
        : ArgumentException(message + " | inner: " + inner.what()) {}

    /// @return The culture name that could not be found, or empty if not set.
    [[nodiscard]] const std::string& getInvalidCultureNameProperty() const { return invalidCultureName_; }
};

} // namespace System::Globalization
