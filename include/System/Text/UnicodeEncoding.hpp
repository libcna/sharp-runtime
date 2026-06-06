// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Text/Encoding.hpp"

namespace System::Text {

    /**
     * @brief Encoding that uses UTF-16 LE (matches .NET default UnicodeEncoding).
     *
     * In this runtime strings are stored as UTF-8 std::string, so GetBytes converts
     * each char to a two-byte little-endian code unit (ASCII-range only, non-ASCII
     * chars produce a replacement pair). GetString reverses the process.
     *
     * @note Status: Partial — ASCII-range only; surrogate pairs not handled.
     */
    class UnicodeEncoding : public Encoding {
    public:
        UnicodeEncoding() = default;

        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& s) const override {
            std::vector<SharpRuntime::bytecs> out;
            out.reserve(s.size() * 2);
            for (unsigned char c : s) {
                out.push_back(static_cast<SharpRuntime::bytecs>(c));
                out.push_back(0);
            }
            return out;
        }

        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* data,
                                            SharpRuntime::intcs index,
                                            SharpRuntime::intcs count) const override {
            std::string out;
            for (SharpRuntime::intcs i = index; i + 1 < index + count; i += 2)
                out += static_cast<char>(data[i]); // low byte only (BMP ASCII)
            return out;
        }

        [[nodiscard]] std::string getEncodingNameProperty() const override { return "utf-16"; }
    };

} // namespace System::Text
