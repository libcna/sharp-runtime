// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/Text/Encoding.hpp"

namespace System::Text {

    // UTF-7 is obsolete (SYSLIB0001); stub for API compatibility only.
    class UTF7Encoding : public Encoding {
        bool allowOptionals_;
    public:
        UTF7Encoding() : allowOptionals_(false) {}
        explicit UTF7Encoding(bool allowOptionals) : allowOptionals_(allowOptionals) {}

        [[nodiscard]] std::string getEncodingNameProperty() const override { return "utf-7"; }
        [[nodiscard]] int getCodePageProperty()             const { return 65000; }
        [[nodiscard]] bool getAllowOptionals()               const { return allowOptionals_; }

        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& s) const override {
            std::vector<SharpRuntime::bytecs> out;
            out.reserve(s.size());
            for (unsigned char c : s) {
                if (c < 0x80) out.push_back(c);
                else          out.push_back('?');
            }
            return out;
        }

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
