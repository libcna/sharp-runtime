// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/Text/Encoding.hpp"

namespace System::Text {

    /// UTF-7 encoding stub (SYSLIB0001 obsolete); provided for API compatibility only.
    class UTF7Encoding : public Encoding {
        bool allowOptionals_;
    public:
        /// Constructs a UTF-7 encoding with optional characters disallowed.
        UTF7Encoding() : allowOptionals_(false) {}
        /// Constructs a UTF-7 encoding with the optional characters setting.
        explicit UTF7Encoding(bool allowOptionals) : allowOptionals_(allowOptionals) {}

        /// Returns the encoding name "utf-7".
        [[nodiscard]] std::string getEncodingNameProperty() const override { return "utf-7"; }
        /// Returns the code page 65000 (UTF-7).
        [[nodiscard]] int getCodePageProperty()             const { return 65000; }
        /// Returns whether optional characters are allowed.
        [[nodiscard]] bool getAllowOptionals()               const { return allowOptionals_; }

        /// Encodes a string to UTF-7 bytes (non-ASCII chars become '?').
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& s) const override {
            std::vector<SharpRuntime::bytecs> out;
            out.reserve(s.size());
            for (unsigned char c : s) {
                if (c < 0x80) out.push_back(c);
                else          out.push_back('?');
            }
            return out;
        }

        /// Decodes a byte range to a string; non-ASCII bytes become '?'.
        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* data,
                                            SharpRuntime::intcs index,
                                            SharpRuntime::intcs count) const override {
            std::string s;
            s.reserve(static_cast<size_t>(count));
            for (SharpRuntime::intcs i = 0; i < count; ++i) {
                uint8_t b = data[index + i];
                s += (b < 0x80) ? static_cast<char>(b) : '?';
            }
            return s;
        }
    };

} // namespace System::Text
