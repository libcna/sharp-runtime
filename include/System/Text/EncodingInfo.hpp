// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include "System/Text/Encoding.hpp"

namespace System::Text {

    /// Provides information about a character encoding (code page, name, display name).
    class EncodingInfo {
        int codePage_;
        std::string name_;
        std::string displayName_;

    public:
        /// Constructs an EncodingInfo with the given code-page identifier, IANA name, and display name.
        EncodingInfo(int codePage, std::string name, std::string displayName)
            : codePage_(codePage), name_(std::move(name)), displayName_(std::move(displayName)) {}

        /// Gets the code-page identifier for this encoding.
        [[nodiscard]] int getCodePageProperty() const { return codePage_; }
        /// Gets the IANA name of this encoding.
        [[nodiscard]] const std::string& getNameProperty() const { return name_; }
        /// Gets the human-readable name of this encoding.
        [[nodiscard]] const std::string& getDisplayNameProperty() const { return displayName_; }

        /// Returns an Encoding object for this EncodingInfo (stub — always returns UTF-8).
        [[nodiscard]] std::shared_ptr<Encoding> GetEncoding() const {
            return Encoding::UTF8();
        }
    };

} // namespace System::Text
