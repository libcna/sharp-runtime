// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
#include "System/ArgumentNullException.hpp"
#include "System/Exception.hpp"
#include "System/String.hpp"
#include <string>

namespace {
    // Ticket #1776 (REMED-CORE-ARGNULL-MESSAGE): a standalone, -Werror public-header
    // consumer verifying ArgumentNullException(paramName) constructs, throws, and is
    // catchable through System::Exception with a single (not doubled) parameter suffix.
    int checkArgumentNullExceptionMessage() {
        try {
            throw System::ArgumentNullException("destination");
        } catch (const System::Exception& exception) {
            const bool paramNameOk =
                dynamic_cast<const System::ArgumentNullException&>(exception)
                    .getParamNameProperty() == "destination";
            const bool messageOk =
                std::string(exception.what()) == "Value cannot be null. (Parameter 'destination')";
            return (paramNameOk && messageOk) ? 0 : 1;
        }
        return 1;
    }
}

int main() {
    if (System::String::IsNullOrEmpty("") == false) return 1;
    return checkArgumentNullExceptionMessage();
}
