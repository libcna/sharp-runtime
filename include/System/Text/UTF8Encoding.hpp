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
        /// Default constructor.
        UTF8Encoding() = default;
        ~UTF8Encoding() override = default;

        /// Encodes a string to a UTF-8 byte sequence.
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& str) const override;
        /// Decodes a UTF-8 byte range to a string.
        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* data,
                                            SharpRuntime::intcs index,
                                            SharpRuntime::intcs count) const override;
        /// Returns the encoding name "utf-8".
        [[nodiscard]] std::string getEncodingNameProperty() const override { return "utf-8"; }
    };

} // namespace System::Text
