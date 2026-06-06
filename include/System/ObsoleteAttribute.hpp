// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System {

    class ObsoleteAttribute : public Attribute {
        std::string message_;
        bool isError_ = false;
        std::string diagnosticId_;
        std::string urlFormat_;

    public:
        ObsoleteAttribute() = default;
        explicit ObsoleteAttribute(const std::string& message) : message_(message) {}
        ObsoleteAttribute(const std::string& message, bool isError) : message_(message), isError_(isError) {}

        [[nodiscard]] const std::string& getMessageProperty()      const { return message_; }
        [[nodiscard]] bool               getIsErrorProperty()      const { return isError_; }
        [[nodiscard]] const std::string& getDiagnosticIdProperty() const { return diagnosticId_; }
        [[nodiscard]] const std::string& getUrlFormatProperty()    const { return urlFormat_; }

        void setDiagnosticIdProperty(const std::string& v) { diagnosticId_ = v; }
        void setUrlFormatProperty(const std::string& v)    { urlFormat_ = v; }
    };

} // namespace System
