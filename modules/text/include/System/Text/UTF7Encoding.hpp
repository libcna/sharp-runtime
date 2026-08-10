// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/Text/Encoding.hpp"
#include "System/Text/Rune.hpp"
#include "System/Text/detail/RawDecodeRange.hpp"

namespace System::Text {

    /**
     * @brief RFC 2152 UTF-7 encoding.
     *
     * This runtime stores strings as UTF-8. UTF-7 therefore converts each decoded
     * scalar value to UTF-16BE code units before representing non-direct runs using
     * modified Base64 (`+...-`, without `=` padding), and reverses that conversion
     * when decoding. `allowOptionals` controls whether RFC 2152's Set O characters
     * are emitted directly; decoders accept direct ASCII characters regardless
     * of that setting, matching the format's permissive input rules.
     *
     * @note UTF-7 is obsolete and unsafe for new protocols (SYSLIB0001 in .NET). It
     * remains here for compatibility with existing content only. Malformed shifted
     * runs and malformed UTF-16 data decode as U+FFFD, preserving valid surrounding
     * text without producing ill-formed UTF-8.
     */
    class UTF7Encoding : public Encoding {
        bool allowOptionals_;

        static constexpr char base64Alphabet_[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        [[nodiscard]] bool isDirect(uint32_t codePoint) const {
            if (codePoint > 0x7F || codePoint == '+') {
                return false;
            }
            if ((codePoint >= 'A' && codePoint <= 'Z') ||
                (codePoint >= 'a' && codePoint <= 'z') ||
                (codePoint >= '0' && codePoint <= '9')) {
                return true;
            }
            switch (codePoint) {
                case '\'': case '(': case ')': case ',': case '-': case '.': case '/':
                case ':': case '?': case ' ': case '\t': case '\r': case '\n':
                    return true;
                default:
                    break;
            }
            if (!allowOptionals_) {
                return false;
            }
            switch (codePoint) {
                case '!': case '"': case '#': case '$': case '%': case '&': case '*':
                case ';': case '<': case '=': case '>': case '@': case '[': case ']':
                case '^': case '_': case '`': case '{': case '|': case '}':
                    return true;
                default:
                    return false;
            }
        }

        static void appendShifted(std::vector<SharpRuntime::bytecs>& output,
                                  const std::vector<uint16_t>& codeUnits,
                                  bool appendTerminator) {
            output.push_back(static_cast<SharpRuntime::bytecs>('+'));
            uint32_t buffer = 0;
            int bitCount = 0;
            for (const uint16_t codeUnit : codeUnits) {
                const uint8_t bytes[] = {
                    static_cast<uint8_t>(codeUnit >> 8),
                    static_cast<uint8_t>(codeUnit)
                };
                for (const uint8_t byte : bytes) {
                    buffer = (buffer << 8) | byte;
                    bitCount += 8;
                    while (bitCount >= 6) {
                        output.push_back(static_cast<SharpRuntime::bytecs>(
                            base64Alphabet_[(buffer >> (bitCount - 6)) & 0x3Fu]));
                        bitCount -= 6;
                        buffer = bitCount == 0 ? 0 : buffer & ((1u << bitCount) - 1u);
                    }
                }
            }
            if (bitCount != 0) {
                output.push_back(static_cast<SharpRuntime::bytecs>(
                    base64Alphabet_[(buffer << (6 - bitCount)) & 0x3Fu]));
            }
            if (appendTerminator) {
                output.push_back(static_cast<SharpRuntime::bytecs>('-'));
            }
        }

        static void appendCodePointAsUtf16(uint32_t codePoint, std::vector<uint16_t>& output) {
            if (codePoint < 0x10000) {
                output.push_back(static_cast<uint16_t>(codePoint));
                return;
            }
            const uint32_t adjusted = codePoint - 0x10000;
            output.push_back(static_cast<uint16_t>(0xD800 + (adjusted >> 10)));
            output.push_back(static_cast<uint16_t>(0xDC00 + (adjusted & 0x3FF)));
        }

        [[nodiscard]] static int base64Value(uint8_t character) {
            if (character >= 'A' && character <= 'Z') return character - 'A';
            if (character >= 'a' && character <= 'z') return character - 'a' + 26;
            if (character >= '0' && character <= '9') return character - '0' + 52;
            if (character == '+') return 62;
            if (character == '/') return 63;
            return -1;
        }

        static void appendUtf8(uint32_t codePoint, std::string& output) {
            if (codePoint < 0x80) {
                output.push_back(static_cast<char>(codePoint));
            } else if (codePoint < 0x800) {
                output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
                output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            } else if (codePoint < 0x10000) {
                output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
                output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            } else {
                output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
                output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
        }

        static void appendReplacement(std::string& output) { appendUtf8(0xFFFD, output); }

        static void appendUtf16Be(const std::vector<uint8_t>& bytes, std::string& output) {
            for (size_t index = 0; index < bytes.size(); index += 2) {
                const uint16_t unit = static_cast<uint16_t>((bytes[index] << 8) | bytes[index + 1]);
                if (unit >= 0xD800 && unit <= 0xDBFF) {
                    if (index + 3 < bytes.size()) {
                        const uint16_t low = static_cast<uint16_t>(
                            (bytes[index + 2] << 8) | bytes[index + 3]);
                        if (low >= 0xDC00 && low <= 0xDFFF) {
                            appendUtf8(0x10000 + ((static_cast<uint32_t>(unit - 0xD800) << 10) |
                                                  (low - 0xDC00)), output);
                            index += 2;
                            continue;
                        }
                    }
                    appendReplacement(output);
                } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
                    appendReplacement(output);
                } else {
                    appendUtf8(unit, output);
                }
            }
        }

    public:
        using Encoding::GetString;

        /** Constructs a UTF-7 encoding with optional characters disallowed. */
        UTF7Encoding() : allowOptionals_(false) {}
        /** Constructs a UTF-7 encoding with the optional characters setting. */
        explicit UTF7Encoding(bool allowOptionals) : allowOptionals_(allowOptionals) {}

        /** Returns the encoding name "utf-7". */
        [[nodiscard]] std::string getEncodingNameProperty() const override { return "utf-7"; }
        /** Returns the code page 65000 (UTF-7). */
        [[nodiscard]] int getCodePageProperty()             const override { return 65000; }
        /** Returns whether optional characters are allowed. */
        [[nodiscard]] bool getAllowOptionals()               const { return allowOptionals_; }

        /**
         * @brief Encodes a UTF-8 string as RFC 2152 UTF-7 bytes.
         *
         * Invalid UTF-8 input is converted to U+FFFD before encoding, matching
         * the runtime's replacement-fallback handling in Unicode encodings.
         */
        [[nodiscard]] std::vector<SharpRuntime::bytecs> GetBytes(const std::string& value) const override {
            std::vector<SharpRuntime::bytecs> output;
            output.reserve(value.size() * 3);
            std::vector<uint16_t> shifted;

            const auto flushShifted = [&output, &shifted](bool appendTerminator) {
                if (!shifted.empty()) {
                    appendShifted(output, shifted, appendTerminator);
                    shifted.clear();
                }
            };

            for (size_t index = 0; index < value.size();) {
                Rune rune;
                size_t consumed = 0;
                uint32_t codePoint = 0xFFFD;
                if (Rune::TryGetRuneAt(value, index, rune, consumed)) {
                    codePoint = rune.getValueProperty();
                }
                if (consumed == 0) {
                    break;
                }
                index += consumed;

                if (codePoint == '+') {
                    flushShifted(true);
                    output.push_back(static_cast<SharpRuntime::bytecs>('+'));
                    output.push_back(static_cast<SharpRuntime::bytecs>('-'));
                } else if (isDirect(codePoint)) {
                    flushShifted(codePoint == '-' || base64Value(static_cast<uint8_t>(codePoint)) >= 0);
                    output.push_back(static_cast<SharpRuntime::bytecs>(codePoint));
                } else {
                    appendCodePointAsUtf16(codePoint, shifted);
                }
            }
            flushShifted(true);
            return output;
        }

        /**
         * @brief Decodes an RFC 2152 UTF-7 byte range to the runtime's UTF-8 string form.
         *
         * A terminating '-' is accepted but not required after a shifted run. Malformed
         * shifted data, non-ASCII input bytes, and invalid UTF-16 surrogate units produce
         * U+FFFD while valid neighboring input continues to decode.
         */
        [[nodiscard]] std::string GetString(const SharpRuntime::bytecs* data,
                                            SharpRuntime::intcs index,
                                            SharpRuntime::intcs count) const override {
            // Ticket #2007 (SR-AUD-286): the four checks that used to be written out here
            // were the ONLY correct raw decode validation in the component, and are now the
            // shared policy the other six overrides adopted. Routing this entry through the
            // same helper keeps one statement of the rule; the behaviour is unchanged, and
            // the tests assert that byte for byte.
            if (!detail::validateRawDecodeRange(data, index, count)) {
                return {};
            }

            std::string output;
            output.reserve(static_cast<size_t>(count));
            size_t offset = 0;
            const size_t length = static_cast<size_t>(count);
            while (offset < length) {
                const uint8_t character = data[index + static_cast<SharpRuntime::intcs>(offset)];
                if (character != '+') {
                    if (character < 0x80) {
                        output.push_back(static_cast<char>(character));
                    } else {
                        appendReplacement(output);
                    }
                    ++offset;
                    continue;
                }

                ++offset;
                if (offset < length && data[index + static_cast<SharpRuntime::intcs>(offset)] == '-') {
                    output.push_back('+');
                    ++offset;
                    continue;
                }

                uint32_t buffer = 0;
                int bitCount = 0;
                bool sawBase64 = false;
                std::vector<uint8_t> bytes;
                while (offset < length) {
                    const int value = base64Value(data[index + static_cast<SharpRuntime::intcs>(offset)]);
                    if (value < 0) {
                        break;
                    }
                    sawBase64 = true;
                    buffer = (buffer << 6) | static_cast<uint32_t>(value);
                    bitCount += 6;
                    while (bitCount >= 8) {
                        bytes.push_back(static_cast<uint8_t>(buffer >> (bitCount - 8)));
                        bitCount -= 8;
                        buffer = bitCount == 0 ? 0 : buffer & ((1u << bitCount) - 1u);
                    }
                    ++offset;
                }
                if (offset < length && data[index + static_cast<SharpRuntime::intcs>(offset)] == '-') {
                    ++offset;
                }

                if (!sawBase64 || bytes.empty() || (bitCount != 0 && buffer != 0) ||
                    (bytes.size() % 2) != 0) {
                    appendReplacement(output);
                } else {
                    appendUtf16Be(bytes, output);
                }
            }
            return output;
        }
    };

} // namespace System::Text
