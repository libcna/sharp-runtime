// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Http/Headers/HttpHeaders.hpp"
#include "System/ArgumentException.hpp"
#include "System/FormatException.hpp"
#include "System/InvalidOperationException.hpp"
#include <algorithm>
#include <cctype>
#include <string_view>

namespace System::Net::Http::Headers {

    namespace {
        bool isHttpTokenChar(unsigned char c) {
            static constexpr std::string_view extras = "!#$%&'*+-.^_`|~";
            return c != 0 && (std::isalnum(c) || extras.find(static_cast<char>(c)) != std::string_view::npos);
        }

        bool isToken(const std::string& s) {
            return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return isHttpTokenChar(static_cast<unsigned char>(c)); });
        }

        // Rejects bare CR/LF/NUL not part of a valid "\r\n " line-folding sequence, matching the
        // established validation pattern from WebHeaderCollection::CheckBadHeaderValueChars.
        void checkValueChars(const std::string& value) {
            int crlf = 0;
            for (unsigned char c : value) {
                switch (crlf) {
                    case 0:
                        if (c == '\r') crlf = 1;
                        else if (c == '\n') crlf = 2;
                        else if (c == 127 || (c < ' ' && c != '\t'))
                            throw System::FormatException("The value contains invalid control characters.");
                        break;
                    case 1:
                        if (c == '\n') { crlf = 2; break; }
                        throw System::FormatException("The value contains invalid CRLF characters.");
                    case 2:
                        if (c == ' ' || c == '\t') { crlf = 0; break; }
                        throw System::FormatException("The value contains invalid control characters.");
                }
            }
            if (crlf != 0) throw System::FormatException("The value contains invalid CRLF characters.");
        }
    }

    void HttpHeaders::Add(const std::string& name, const std::string& value) {
        System::ArgumentException::ThrowIfNullOrEmpty(name, "name");
        if (!isToken(name)) {
            throw System::FormatException("The header name is not a valid HTTP token: " + name);
        }
        checkValueChars(value);
        headers_.Add(name, value);
    }

    void HttpHeaders::Add(const std::string& name, const std::vector<std::string>& values) {
        for (const auto& v : values) Add(name, v);
    }

    bool HttpHeaders::TryAddWithoutValidation(const std::string& name, const std::string& value) {
        if (name.empty()) return false;
        headers_.Add(name, value);
        return true;
    }

    bool HttpHeaders::TryAddWithoutValidation(const std::string& name, const std::vector<std::string>& values) {
        if (name.empty()) return false;
        for (const auto& v : values) headers_.Add(name, v);
        return true;
    }

    std::vector<std::string> HttpHeaders::GetValues(const std::string& name) const {
        std::vector<std::string> values;
        if (!TryGetValues(name, values)) {
            throw System::InvalidOperationException("The header '" + name + "' was not found.");
        }
        return values;
    }

    bool HttpHeaders::TryGetValues(const std::string& name, std::vector<std::string>& values) const {
        if (!Contains(name)) return false;
        values = headers_.GetValues(name);
        return true;
    }

    bool HttpHeaders::Contains(const std::string& name) const {
        return std::any_of(headers_.AllKeys().begin(), headers_.AllKeys().end(), [&](const std::string& k) {
            return k.size() == name.size() && std::equal(k.begin(), k.end(), name.begin(), [](unsigned char a, unsigned char b) {
                return std::tolower(a) == std::tolower(b);
            });
        });
    }

    bool HttpHeaders::Remove(const std::string& name) {
        if (!Contains(name)) return false;
        headers_.Remove(name);
        return true;
    }

    void HttpHeaders::Clear() {
        headers_.Clear();
    }

    SharpRuntime::intcs HttpHeaders::getCountProperty() const {
        return headers_.getCountProperty();
    }

    const std::vector<std::string>& HttpHeaders::getHeaderNamesProperty() const {
        return headers_.AllKeys();
    }

    std::string HttpHeaders::ToString() const {
        std::string result;
        for (const auto& key : headers_.AllKeys()) {
            result += key + ": " + headers_.Get(key) + "\r\n";
        }
        result += "\r\n";
        return result;
    }

    std::string HttpHeaders::getRawValue(const std::string& name) const {
        return Contains(name) ? headers_.Get(name) : "";
    }

    void HttpHeaders::setRawValue(const std::string& name, const std::string& value) {
        if (value.empty()) {
            Remove(name);
        } else {
            headers_.Set(name, value);
        }
    }

} // namespace System::Net::Http::Headers
