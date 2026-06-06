// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System::Diagnostics {

    class ConditionalAttribute : public System::Attribute {
        std::string conditionString_;
    public:
        explicit ConditionalAttribute(const std::string& conditionString)
            : conditionString_(conditionString) {}
        [[nodiscard]] const std::string& getConditionStringProperty() const { return conditionString_; }
    };

} // namespace System::Diagnostics
