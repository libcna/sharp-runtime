// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Text/Encoding.hpp"

namespace System::Text {

    /**
     * @brief Represents a UTF-8 encoding of Unicode characters.
     *
     * Partial C++ counterpart of .NET System.Text.UTF8Encoding.
     *
     * @note Status: Implemented
     */
    class UTF8Encoding : public Encoding {
    public:
        UTF8Encoding() = default;
        ~UTF8Encoding() override = default;

        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& str) const override;
        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* data,
                                            SharpRuntime::intcs index,
                                            SharpRuntime::intcs count) const override;
        [[nodiscard]] std::string getEncodingNameProperty() const override { return "utf-8"; }
    };

} // namespace System::Text
