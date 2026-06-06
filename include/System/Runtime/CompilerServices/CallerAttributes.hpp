// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Attribute.hpp"

namespace System::Runtime::CompilerServices {

    // C++ equivalents: use __FUNCTION__, __FILE__, __LINE__ directly.
    class CallerMemberNameAttribute : public System::Attribute {};
    class CallerFilePathAttribute   : public System::Attribute {};
    class CallerLineNumberAttribute : public System::Attribute {};
    class CallerArgumentExpressionAttribute : public System::Attribute {
        std::string parameterName_;
    public:
        explicit CallerArgumentExpressionAttribute(const std::string& parameterName)
            : parameterName_(parameterName) {}
        [[nodiscard]] const std::string& getParameterNameProperty() const { return parameterName_; }
    };

} // namespace System::Runtime::CompilerServices
