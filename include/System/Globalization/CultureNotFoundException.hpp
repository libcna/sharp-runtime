// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/ArgumentException.hpp"

namespace System::Globalization {

    class CultureNotFoundException : public System::ArgumentException {
        std::string invalidCultureName_;
    public:
        CultureNotFoundException() : ArgumentException("Culture is not supported.") {}
        explicit CultureNotFoundException(const std::string& message) : ArgumentException(message) {}
        CultureNotFoundException(const std::string& message, const std::string& invalidCultureName)
            : ArgumentException(message, invalidCultureName), invalidCultureName_(invalidCultureName) {}
        CultureNotFoundException(const std::string& message, const std::exception& inner)
            : ArgumentException(message + " | inner: " + inner.what()) {}

        [[nodiscard]] const std::string& getInvalidCultureNameProperty() const { return invalidCultureName_; }
    };

} // namespace System::Globalization
