// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/ArgumentOutOfRangeException.hpp"

namespace System::Text::Encodings::Web {

    using SharpRuntime::intcs;

    /** Encodes strings for safe embedding in HTML by escaping special characters. */
    class HtmlEncoder {
    public:
        /** HTML-encodes the entire string (& < > " ' are escaped). */
        static std::string Encode(const std::string& value) {
            std::string out;
            out.reserve(value.size());
            for (char c : value) {
                switch (c) {
                    case '&':  out += "&amp;";  break;
                    case '<':  out += "&lt;";   break;
                    case '>':  out += "&gt;";   break;
                    case '"':  out += "&quot;"; break;
                    case '\'': out += "&#x27;"; break;
                    default:   out += c;        break;
                }
            }
            return out;
        }

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
