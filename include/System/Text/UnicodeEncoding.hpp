// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "System/Text/Encoding.hpp"

namespace System::Text {

    /**
     * @brief Encoding that uses UTF-16 (matches .NET's UnicodeEncoding — little-endian by
     * default, or big-endian).
     *
     * This runtime represents `System::String`/`std::string` as UTF-8 internally, so GetBytes()
     * decodes the source UTF-8 sequence into Unicode scalar values and re-encodes each as one or
     * two UTF-16 code units (surrogate pairs for U+10000 and above); GetString() reverses this.
     * Unlike an earlier, simpler version of this file, this handles the full Unicode range, not
     * just ASCII.
     *
     * C++ counterpart of .NET System.Text.UnicodeEncoding.
     */
    class UnicodeEncoding : public Encoding {
        bool bigEndian_;
        bool byteOrderMark_;

    public:
        /** Constructs a UTF-16 LE encoding with no byte-order mark. */
        UnicodeEncoding() : bigEndian_(false), byteOrderMark_(false) {}
        /** Constructs a UTF-16 encoding with the specified endianness and BOM setting. */
        UnicodeEncoding(bool bigEndian, bool byteOrderMark) : bigEndian_(bigEndian), byteOrderMark_(byteOrderMark) {}

        /** Returns the encoding name ("utf-16" or "utf-16BE"). */
        [[nodiscard]] std::string getEncodingNameProperty() const override { return bigEndian_ ? "utf-16BE" : "utf-16"; }
        /** Returns the code page (1201 for big-endian, 1200 for little-endian). */
        [[nodiscard]] int getCodePageProperty() const override { return bigEndian_ ? 1201 : 1200; }

        /** @return true if this instance encodes as big-endian UTF-16. */
        [[nodiscard]] bool getIsBigEndianProperty() const { return bigEndian_; }
        /** @return true if GetBytes() prepends a byte-order mark. */
        [[nodiscard]] bool getByteOrderMarkProperty() const { return byteOrderMark_; }

        /** Encodes a string (decoded from UTF-8) to UTF-16 bytes, in the configured endianness. */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& s) const override {
            std::vector<SharpRuntime::bytecs> out;
            out.reserve(s.size() * 2 + 2);
            auto writeUnit = [&](uint16_t unit) {
                if (bigEndian_) {
                    out.push_back(static_cast<SharpRuntime::bytecs>((unit >> 8) & 0xFF));
                    out.push_back(static_cast<SharpRuntime::bytecs>(unit & 0xFF));
                } else {
                    out.push_back(static_cast<SharpRuntime::bytecs>(unit & 0xFF));
                    out.push_back(static_cast<SharpRuntime::bytecs>((unit >> 8) & 0xFF));
                }
            };
            if (byteOrderMark_) writeUnit(0xFEFF);

            size_t i = 0;
            while (i < s.size()) {
                uint32_t cp;
                size_t len;
                decodeUtf8(s, i, cp, len);
                i += len;
                if (cp < 0x10000) {
                    writeUnit(static_cast<uint16_t>(cp));
                } else {
                    uint32_t v = cp - 0x10000;
                    writeUnit(static_cast<uint16_t>(0xD800 + (v >> 10)));
                    writeUnit(static_cast<uint16_t>(0xDC00 + (v & 0x3FF)));
                }
            }
            return out;
        }

        /** Decodes a UTF-16 byte range (in the configured endianness) to a UTF-8-encoded string. */
        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* data,
                                            SharpRuntime::intcs index,
                                            SharpRuntime::intcs count) const override {
            std::string out;
            SharpRuntime::intcs i = index;
            SharpRuntime::intcs end = index + count;

            if (count >= 2) {
                uint16_t bom = readUnit(data, i);
                if (bom == 0xFEFF) i += 2;
            }

            while (i + 1 < end) {
                uint16_t unit = readUnit(data, i);
                i += 2;
                uint32_t cp = unit;
                if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < end) {
                    uint16_t low = readUnit(data, i);
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        cp = 0x10000 + ((static_cast<uint32_t>(unit - 0xD800) << 10) | (low - 0xDC00));
                        i += 2;
                    }
                }
                encodeUtf8(cp, out);
            }
            return out;
        }

    private:
        [[nodiscard]] uint16_t readUnit(const SharpRuntime::bytecs* data, SharpRuntime::intcs i) const {
            uint8_t b0 = data[i];
            uint8_t b1 = data[i + 1];
            return bigEndian_ ? static_cast<uint16_t>((b0 << 8) | b1) : static_cast<uint16_t>((b1 << 8) | b0);
        }

        static void decodeUtf8(const std::string& s, size_t i, uint32_t& codePoint, size_t& length) {
            unsigned char c0 = static_cast<unsigned char>(s[i]);
            if (c0 < 0x80) {
                codePoint = c0;
                length = 1;
            } else if ((c0 & 0xE0) == 0xC0 && i + 1 < s.size()) {
                codePoint = ((c0 & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
                length = 2;
            } else if ((c0 & 0xF0) == 0xE0 && i + 2 < s.size()) {
                codePoint = ((c0 & 0x0F) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                            (static_cast<unsigned char>(s[i + 2]) & 0x3F);
                length = 3;
            } else if ((c0 & 0xF8) == 0xF0 && i + 3 < s.size()) {
                codePoint = ((c0 & 0x07) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                            ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) | (static_cast<unsigned char>(s[i + 3]) & 0x3F);
                length = 4;
            } else {
                codePoint = 0xFFFD;
                length = 1;
            }
        }

        static void encodeUtf8(uint32_t cp, std::string& out) {
            if (cp < 0x80) {
                out += static_cast<char>(cp);
            } else if (cp < 0x800) {
                out += static_cast<char>(0xC0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                out += static_cast<char>(0xE0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                out += static_cast<char>(0xF0 | (cp >> 18));
                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }
    };

} // namespace System::Text
