// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System::Diagnostics {

    class DebuggerTypeProxyAttribute : public System::Attribute {
        std::string proxyTypeName_;
        std::string target_;

    public:
        explicit DebuggerTypeProxyAttribute(const std::string& typeName) : proxyTypeName_(typeName) {}

        [[nodiscard]] const std::string& getProxyTypeNameProperty() const { return proxyTypeName_; }
        [[nodiscard]] const std::string& getTargetProperty()        const { return target_; }
        void setTargetProperty(const std::string& v) { target_ = v; }
    };

} // namespace System::Diagnostics
