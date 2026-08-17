// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include "System/Text/detail/Utf8Scalar.hpp"
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System::Text::Encodings::Web {

    using SharpRuntime::intcs;

    /** Encodes strings for safe embedding in HTML by escaping special characters. */
    class HtmlEncoder {
    public:
        /**
         * @brief HTML-encodes @p value: the five markup characters, plus every scalar outside
         * Basic Latin.
         *
         * Ticket #2019 (SR-AUD-297, policy half). Before it this escaped only
         * `& < > " '` and passed every non-ASCII scalar through unchanged, so
         * `Encode(u8"é")` returned `c3 a9`. .NET's default encoder is built with an **allow-list**
         * and escapes everything outside it: `DefaultHtmlEncoder.BasicLatinSingleton` is
         * `new DefaultHtmlEncoder(new TextEncoderSettings(UnicodeRanges.BasicLatin))`
         * (`DefaultHtmlEncoder.cs:13`), i.e. U+0000..U+007F is allowed and nothing else is.
         *
         * The escape form is .NET's: `&#x` followed by the **scalar** in uppercase hex with no
         * padding, then `;` (`DefaultHtmlEncoder.cs:98-126`) — so U+00E9 is `&#xE9;` and U+1F600
         * is `&#x1F600;`, one escape for the whole scalar rather than one per surrogate half.
         *
         * @note This is a **conservative** default, and deliberately so: an allow-list is safe
         *       against a character nobody thought about, which a deny-list is not. A caller who
         *       wants non-ASCII text to survive unescaped needs a relaxed encoder, which .NET
         *       also requires and which this port does not yet provide (ticket #2011 records the
         *       diagnostics half of this finding).
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
                switch (cp) {
                    case '&':  out += "&amp;";  continue;
                    case '<':  out += "&lt;";   continue;
                    case '>':  out += "&gt;";   continue;
                    case '"':  out += "&quot;"; continue;
                    case '\'': out += "&#x27;"; continue;
                    default: break;
                }
                if (cp <= 0x7F) {
                    out += static_cast<char>(cp);
                } else {
                    out += "&#x";
                    out += toUpperHex(cp);
                    out += ';';
                }
            }
            return out;
        }

    private:
        /** Uppercase hex with no padding, matching .NET's `&#x…;` form. */
        static std::string toUpperHex(std::uint32_t value) {
            static const char* const digits = "0123456789ABCDEF";
            if (value == 0) return "0";
            std::string text;
            while (value != 0) {
                text.insert(text.begin(), digits[value & 0xF]);
                value >>= 4;
            }
            return text;
        }

    public:
        /** Returns the default HtmlEncoder singleton. */
        static const HtmlEncoder& Default() {
            static HtmlEncoder instance;
            return instance;
        }

        /**
         * HTML-encodes a substring defined by startIndex and characterCount.
         *
         * @throws System::ArgumentOutOfRangeException if @p startIndex or @p characterCount
         *         is negative, or if the range extends past the end of @p value.
         *
         * @note Ticket #2011 (SR-AUD-297, cause T-E). The body forwarded both arguments to
         *       `std::string::substr` after an unsigned conversion, so a negative
         *       @p startIndex became `18446744073709551615` and `substr` threw
         *       `std::out_of_range` — a `std::` exception escaping a `System`-shaped API —
         *       while an over-long @p characterCount was **silently clamped** to the end of
         *       the string instead of being rejected. Both measured in
         *       `build-probe/2006_probe1_before.log` §L.
         *
         * @note @p startIndex and @p characterCount are UTF-8 storage-byte positions, like
         *       every other index in this component; see
         *       `docs/SystemTextNamespaceReviewPlan.md` §14.3 (ticket #2015).
         */
        std::string Encode(const std::string& value, intcs startIndex, intcs characterCount) const {
            if (startIndex < 0)
                throw System::ArgumentOutOfRangeException("startIndex", "Non-negative number required.");
            if (characterCount < 0)
                throw System::ArgumentOutOfRangeException("characterCount", "Non-negative number required.");
            const auto size = static_cast<long long>(value.size());
            if (static_cast<long long>(startIndex) + static_cast<long long>(characterCount) > size)
                throw System::ArgumentOutOfRangeException(
                    "characterCount", "Index and count must refer to a location within the string.");
            return Encode(value.substr(static_cast<std::size_t>(startIndex),
                                       static_cast<std::size_t>(characterCount)));
        }
    };

} // namespace System::Text::Encodings::Web
