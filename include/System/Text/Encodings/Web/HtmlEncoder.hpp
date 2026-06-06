// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System::Text::Encodings::Web {

    class HtmlEncoder {
    public:
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

        static const HtmlEncoder& Default() {
            static HtmlEncoder instance;
            return instance;
        }

        std::string Encode(const std::string& value, std::size_t startIndex, int characterCount) const {
            return Encode(value.substr(startIndex, static_cast<std::size_t>(characterCount)));
        }
    };

} // namespace System::Text::Encodings::Web
