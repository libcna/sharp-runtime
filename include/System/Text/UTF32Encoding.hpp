// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <vector>
#include "System/Text/Encoding.hpp"

namespace System::Text {

    /** UTF-32 encoding; defaults to little-endian with byte-order mark. */
    class UTF32Encoding : public Encoding {
        bool bigEndian_;
        bool byteOrderMark_;

    public:
        /** Constructs a UTF-32 LE encoding with a byte-order mark. */
        UTF32Encoding() : bigEndian_(false), byteOrderMark_(true) {}
        /** Constructs a UTF-32 encoding with the specified endianness and BOM setting. */
        UTF32Encoding(bool bigEndian, bool byteOrderMark)
            : bigEndian_(bigEndian), byteOrderMark_(byteOrderMark) {}

        /** Returns the encoding name "utf-32". */
        [[nodiscard]] std::string getEncodingNameProperty() const override { return "utf-32"; }
        /** Returns the code page 12000 (UTF-32 LE). */
        [[nodiscard]] int getCodePageProperty() const override { return 12000; }

        /** Encodes a string to a UTF-32 byte sequence. */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& s) const override {
            std::vector<SharpRuntime::bytecs> result;
            if (byteOrderMark_ && !bigEndian_) {
                // LE BOM: FF FE 00 00
                result.insert(result.end(), {0xFF, 0xFE, 0x00, 0x00});
            }
            for (unsigned char c : s) {
                uint32_t cp = static_cast<uint32_t>(c);
                if (!bigEndian_) {
                    result.push_back(static_cast<SharpRuntime::bytecs>(cp & 0xFF));
                    result.push_back(static_cast<SharpRuntime::bytecs>((cp >> 8) & 0xFF));
                    result.push_back(static_cast<SharpRuntime::bytecs>((cp >> 16) & 0xFF));
                    result.push_back(static_cast<SharpRuntime::bytecs>((cp >> 24) & 0xFF));
                } else {
                    result.push_back(static_cast<SharpRuntime::bytecs>((cp >> 24) & 0xFF));
                    result.push_back(static_cast<SharpRuntime::bytecs>((cp >> 16) & 0xFF));
                    result.push_back(static_cast<SharpRuntime::bytecs>((cp >> 8) & 0xFF));
                    result.push_back(static_cast<SharpRuntime::bytecs>(cp & 0xFF));
                }
            }
            return result;
        }

        /** Decodes a UTF-32 byte range to a string (ASCII range only in this partial implementation). */
        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* data,
                                             SharpRuntime::intcs index,
                                             SharpRuntime::intcs count) const override {
            std::string result;
            int start = index;
            // skip BOM if present
            if (count >= 4) {
                uint8_t b0 = data[start], b1 = data[start+1], b2 = data[start+2], b3 = data[start+3];
                if (!bigEndian_ && b0==0xFF && b1==0xFE && b2==0x00 && b3==0x00) { start += 4; count -= 4; }
                if (bigEndian_  && b0==0x00 && b1==0x00 && b2==0xFE && b3==0xFF) { start += 4; count -= 4; }
            }
            for (int i = start; i + 3 < start + count; i += 4) {
                uint32_t cp;
                if (!bigEndian_)
                    cp = static_cast<uint8_t>(data[i]) | (static_cast<uint8_t>(data[i+1]) << 8)
                       | (static_cast<uint8_t>(data[i+2]) << 16) | (static_cast<uint8_t>(data[i+3]) << 24);
                else
                    cp = (static_cast<uint8_t>(data[i]) << 24) | (static_cast<uint8_t>(data[i+1]) << 16)
                       | (static_cast<uint8_t>(data[i+2]) << 8) | static_cast<uint8_t>(data[i+3]);
                if (cp < 0x80) result += static_cast<char>(cp);
                // Only ASCII range decoded in this simplified implementation
            }
            return result;
        }
    };

} // namespace System::Text
