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
     * @brief UTF-32 encoding; defaults to little-endian with a byte-order mark.
     *
     * This runtime represents `System::String`/`std::string` as UTF-8 internally, so GetBytes()
     * decodes the source UTF-8 sequence into Unicode scalar values and re-encodes each as one
     * 4-byte UTF-32 code unit; GetString() reverses this — the full Unicode range is supported,
     * not just ASCII.
     *
     * C++ counterpart of .NET System.Text.UTF32Encoding.
     */
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
        /** Returns the code page (12001 for big-endian, 12000 for little-endian). */
        [[nodiscard]] int getCodePageProperty() const override { return bigEndian_ ? 12001 : 12000; }

        /** @return true if this instance encodes as big-endian UTF-32. */
        [[nodiscard]] bool getIsBigEndianProperty() const { return bigEndian_; }
        /** @return true if GetBytes() prepends a byte-order mark. */
        [[nodiscard]] bool getByteOrderMarkProperty() const { return byteOrderMark_; }

        /** Encodes a string (decoded from UTF-8) to a UTF-32 byte sequence. */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& s) const override {
            std::vector<SharpRuntime::bytecs> result;
            result.reserve(s.size() * 4 + 4);
            if (byteOrderMark_) {
                writeUnit(result, 0x0000FEFF);
            }

            size_t i = 0;
            while (i < s.size()) {
                uint32_t cp;
                size_t len;
                decodeUtf8(s, i, cp, len);
                i += len;
                writeUnit(result, cp);
            }
            return result;
        }

        /** Decodes a UTF-32 byte range to a UTF-8-encoded string. */
        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* data,
                                             SharpRuntime::intcs index,
                                             SharpRuntime::intcs count) const override {
            std::string result;
            SharpRuntime::intcs start = index;
            if (count >= 4) {
                uint32_t maybeBom = readUnit(data, start);
                if (maybeBom == 0x0000FEFF) {
                    start += 4;
                    count -= 4;
                }
            }
            for (SharpRuntime::intcs i = start; i + 3 < start + count; i += 4) {
                uint32_t cp = readUnit(data, i);
                encodeUtf8(cp, result);
            }
            return result;
        }

    private:
        void writeUnit(std::vector<SharpRuntime::bytecs>& out, uint32_t cp) const {
            if (!bigEndian_) {
                out.push_back(static_cast<SharpRuntime::bytecs>(cp & 0xFF));
                out.push_back(static_cast<SharpRuntime::bytecs>((cp >> 8) & 0xFF));
                out.push_back(static_cast<SharpRuntime::bytecs>((cp >> 16) & 0xFF));
                out.push_back(static_cast<SharpRuntime::bytecs>((cp >> 24) & 0xFF));
            } else {
                out.push_back(static_cast<SharpRuntime::bytecs>((cp >> 24) & 0xFF));
                out.push_back(static_cast<SharpRuntime::bytecs>((cp >> 16) & 0xFF));
                out.push_back(static_cast<SharpRuntime::bytecs>((cp >> 8) & 0xFF));
                out.push_back(static_cast<SharpRuntime::bytecs>(cp & 0xFF));
            }
        }

        [[nodiscard]] uint32_t readUnit(const SharpRuntime::bytecs* data, SharpRuntime::intcs i) const {
            uint8_t b0 = data[i], b1 = data[i + 1], b2 = data[i + 2], b3 = data[i + 3];
            if (!bigEndian_) {
                return static_cast<uint32_t>(b0) | (static_cast<uint32_t>(b1) << 8) | (static_cast<uint32_t>(b2) << 16) |
                       (static_cast<uint32_t>(b3) << 24);
            }
            return (static_cast<uint32_t>(b0) << 24) | (static_cast<uint32_t>(b1) << 16) | (static_cast<uint32_t>(b2) << 8) |
                   static_cast<uint32_t>(b3);
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
