// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include "System/Text/Encoding.hpp"

namespace System::Text {

    class EncodingInfo {
        int codePage_;
        std::string name_;
        std::string displayName_;

    public:
        EncodingInfo(int codePage, std::string name, std::string displayName)
            : codePage_(codePage), name_(std::move(name)), displayName_(std::move(displayName)) {}

        [[nodiscard]] int getCodePageProperty() const { return codePage_; }
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }
        [[nodiscard]] const std::string& getDisplayNameProperty() const { return displayName_; }

        [[nodiscard]] std::shared_ptr<Encoding> GetEncoding() const {
            return Encoding::UTF8();
        }
    };

} // namespace System::Text
