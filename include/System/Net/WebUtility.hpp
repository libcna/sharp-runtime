// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <iomanip>
#include <sstream>
#include <string>

namespace System::Net {

    class WebUtility {
    public:
        WebUtility() = delete;

        static std::string HtmlEncode(const std::string& value) {
            std::string out;
            out.reserve(value.size());
            for (unsigned char c : value) {
                switch (c) {
                    case '&':  out += "&amp;";  break;
                    case '<':  out += "&lt;";   break;
                    case '>':  out += "&gt;";   break;
                    case '"':  out += "&quot;"; break;
                    case '\'': out += "&#39;";  break;
                    default:   out += c;        break;
                }
            }
            return out;
        }

        static std::string HtmlDecode(const std::string& value) {
            std::string out;
            out.reserve(value.size());
            std::size_t i = 0;
            while (i < value.size()) {
                if (value[i] != '&') { out += value[i++]; continue; }
                std::size_t semi = value.find(';', i);
                if (semi == std::string::npos) { out += value[i++]; continue; }
                std::string entity = value.substr(i + 1, semi - i - 1);
                if      (entity == "amp")  out += '&';
                else if (entity == "lt")   out += '<';
                else if (entity == "gt")   out += '>';
                else if (entity == "quot") out += '"';
                else if (entity == "#39")  out += '\'';
                else { out += value.substr(i, semi - i + 1); }
                i = semi + 1;
            }
            return out;
        }

        static std::string UrlEncode(const std::string& value) {
            std::ostringstream oss;
            for (unsigned char c : value) {
                if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                    oss << c;
                else if (c == ' ')
                    oss << '+';
                else
                    oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)c;
            }
            return oss.str();
        }

        static std::string UrlDecode(const std::string& value) {
            std::string out;
            for (std::size_t i = 0; i < value.size(); ++i) {
                if (value[i] == '+') { out += ' '; }
                else if (value[i] == '%' && i + 2 < value.size()) {
                    int hex = std::stoi(value.substr(i + 1, 2), nullptr, 16);
                    out += static_cast<char>(hex);
                    i += 2;
                } else {
                    out += value[i];
                }
            }
            return out;
        }
    };

} // namespace System::Net
