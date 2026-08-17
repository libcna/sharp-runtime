// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include "System/Text/detail/Utf8Scalar.hpp"
#include <iomanip>
#include <sstream>
#include <string>

namespace System::Text::Encodings::Web {

    /** Encodes strings for safe embedding in JavaScript literals. */
    class JavaScriptEncoder {
    public:
        /** Escapes special characters in the given string for JavaScript embedding. */
        /**
         * @brief JavaScript-encodes @p value: the usual escapes, plus every scalar outside
         * Basic Latin.
         *
         * Ticket #2019 (SR-AUD-297, policy half). Before it this passed every non-ASCII scalar
         * through unchanged. .NET's default encoder is built with an **allow-list** --
         * `DefaultJavaScriptEncoder.BasicLatinSingleton` is
         * `new DefaultJavaScriptEncoder(new TextEncoderSettings(UnicodeRanges.BasicLatin))`
         * (`DefaultJavaScriptEncoder.cs:11`) -- so U+0000..U+007F is allowed and nothing else is.
         *
         * The escape form is .NET's (`DefaultJavaScriptEncoder.cs:129-150`): a BMP scalar becomes
         * `\uXXXX` with four uppercase hex digits, and a supplementary scalar becomes its
         * **surrogate pair**, two `\uXXXX` escapes -- because that is what a JavaScript string
         * literal can express.
         *
         * @note A conservative default, deliberately: an allow-list is safe against a character
         *       nobody thought about, which a deny-list is not. #2011 records the diagnostics
         *       half of this finding.
         */
        static std::string Encode(const std::string& value) {
            std::string out;
            out.reserve(value.size());
            std::size_t i = 0;
            while (i < value.size()) {
                std::uint32_t cp = 0;
                std::size_t len = 0;
                System::Text::detail::DecodeUtf8Scalar(value, i, cp, len);
                i += len;
                if (cp == '\\') { out += "\\\\"; continue; }
                if (cp == '"')  { out += "\\\""; continue; }
                if (cp == '\n') { out += "\\n";  continue; }
                if (cp == '\r') { out += "\\r";  continue; }
                if (cp == '\t') { out += "\\t";  continue; }
                if (cp >= 0x20 && cp <= 0x7F) { out += static_cast<char>(cp); continue; }
                if (cp < 0x10000) {
                    appendUnitEscape(out, static_cast<std::uint16_t>(cp));
                } else {
                    const std::uint32_t v = cp - 0x10000;
                    appendUnitEscape(out, static_cast<std::uint16_t>(0xD800 + (v >> 10)));
                    appendUnitEscape(out, static_cast<std::uint16_t>(0xDC00 + (v & 0x3FF)));
                }
            }
            return out;
        }

    private:
        /** `\uXXXX`, four uppercase hex digits -- .NET's form. */
        static void appendUnitEscape(std::string& out, std::uint16_t unit) {
            static const char* const digits = "0123456789ABCDEF";
            out += "\\u";
            out += digits[(unit >> 12) & 0xF];
            out += digits[(unit >> 8) & 0xF];
            out += digits[(unit >> 4) & 0xF];
            out += digits[unit & 0xF];
        }

    public:
        /** Returns the default JavaScriptEncoder singleton. */
        static const JavaScriptEncoder& Default() {
            static JavaScriptEncoder instance;
            return instance;
        }
    };

} // namespace System::Text::Encodings::Web
