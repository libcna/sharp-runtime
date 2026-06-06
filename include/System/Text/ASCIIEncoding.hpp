// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

#include "System/Text/Encoding.hpp"

namespace System::Text {

    /**
     * @brief Represents the ASCII character encoding.
     * Characters outside 0–127 are replaced by '?'.
     *
     * Partial C++ counterpart of .NET System.Text.ASCIIEncoding.
     *
     * @note Status: Implemented
     */
    class ASCIIEncoding : public Encoding {
    public:
        ASCIIEncoding() = default;
        ~ASCIIEncoding() override = default;

        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& str) const override;
        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* data,
                                            SharpRuntime::intcs index,
                                            SharpRuntime::intcs count) const override;
        [[nodiscard]] std::string getEncodingNameProperty() const override { return "us-ascii"; }
    };

} // namespace System::Text
