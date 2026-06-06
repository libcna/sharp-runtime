// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/Attribute.hpp"

namespace System::Diagnostics {

    class DebuggerVisualizerAttribute : public System::Attribute {
        std::string visualizerTypeName_;
        std::string visualizerObjectSourceTypeName_;
        std::string description_;
        std::string target_;

    public:
        explicit DebuggerVisualizerAttribute(const std::string& visualizerTypeName)
            : visualizerTypeName_(visualizerTypeName) {}
        DebuggerVisualizerAttribute(const std::string& visualizerTypeName,
                                    const std::string& visualizerObjectSourceTypeName)
            : visualizerTypeName_(visualizerTypeName),
              visualizerObjectSourceTypeName_(visualizerObjectSourceTypeName) {}

        [[nodiscard]] const std::string& getVisualizerTypeNameProperty()             const { return visualizerTypeName_; }
        [[nodiscard]] const std::string& getVisualizerObjectSourceTypeNameProperty() const { return visualizerObjectSourceTypeName_; }
        [[nodiscard]] const std::string& getDescriptionProperty()                    const { return description_; }
        [[nodiscard]] const std::string& getTargetProperty()                         const { return target_; }

        void setDescriptionProperty(const std::string& v) { description_ = v; }
        void setTargetProperty(const std::string& v)      { target_ = v; }
    };

} // namespace System::Diagnostics
