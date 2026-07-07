// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Http/Headers/NameValueHeaderValue.hpp"
#include "System/ArgumentException.hpp"
#include "System/FormatException.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace System::Net::Http::Headers {

    namespace {
        bool isHttpTokenChar(unsigned char c) {
            static constexpr const char* extras = "!#$%&'*+-.^_`|~";
            return c != 0 && (std::isalnum(c) || std::strchr(extras, static_cast<char>(c)) != nullptr);
        }

        bool isToken(const std::string& s) {
            return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return isHttpTokenChar(static_cast<unsigned char>(c)); });
        }

        bool equalsIgnoreCase(const std::string& a, const std::string& b) {
            if (a.size() != b.size()) return false;
            return std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
                return std::tolower(x) == std::tolower(y);
            });
        }

        SharpRuntime::intcs hashIgnoreCase(const std::string& s) {
            std::string lower = s;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(lower));
        }
    }

    bool NameValueHeaderValue::isValidQuotedString(const std::string& value) {
        if (value.size() < 2 || value.front() != '"' || value.back() != '"') return false;
        for (size_t i = 1; i + 1 < value.size(); ) {
            if (value[i] == '\\') {
                if (i + 1 >= value.size() - 1) return false;
                i += 2;
            } else if (value[i] == '"') {
                return false;
            } else {
                ++i;
            }
        }
        return true;
    }

    void NameValueHeaderValue::checkValidToken(const std::string& value, const std::string& paramName) {
        System::ArgumentException::ThrowIfNullOrEmpty(value, paramName);
        if (!isToken(value)) {
            throw System::FormatException("The value is not a valid HTTP token: " + value);
        }
    }

    void NameValueHeaderValue::checkValueFormat(const std::string& value) {
        if (value.empty()) return;

        if (value.front() == ' ' || value.front() == '\t' || value.back() == ' ' || value.back() == '\t') {
            throw System::FormatException("The value is not valid: " + value);
        }

        if (value.front() == '"') {
            if (!isValidQuotedString(value)) {
                throw System::FormatException("The value is not a valid quoted-string: " + value);
            }
        } else if (value.find('\r') != std::string::npos || value.find('\n') != std::string::npos || value.find('\0') != std::string::npos) {
            throw System::FormatException("The value is not valid: " + value);
        }
    }

    NameValueHeaderValue::NameValueHeaderValue(const std::string& name) : name_(name) {
        checkValidToken(name, "name");
    }

    NameValueHeaderValue::NameValueHeaderValue(const std::string& name, const std::string& value)
        : name_(name), value_(value) {
        checkValidToken(name, "name");
        checkValueFormat(value);
    }

    void NameValueHeaderValue::setValueProperty(const std::string& value) {
        checkValueFormat(value);
        value_ = value;
    }

    bool NameValueHeaderValue::Equals(const NameValueHeaderValue& other) const {
        if (!equalsIgnoreCase(name_, other.name_)) return false;

        if (value_.empty()) return other.value_.empty();
        if (other.value_.empty()) return false;

        if (value_.front() == '"') {
            return value_ == other.value_;
        }
        return equalsIgnoreCase(value_, other.value_);
    }

    SharpRuntime::intcs NameValueHeaderValue::GetHashCode() const {
        SharpRuntime::intcs nameHash = hashIgnoreCase(name_);
        if (value_.empty()) return nameHash;

        if (value_.front() == '"') {
            return nameHash ^ static_cast<SharpRuntime::intcs>(std::hash<std::string>{}(value_));
        }
        return nameHash ^ hashIgnoreCase(value_);
    }

    std::string NameValueHeaderValue::ToString() const {
        if (!value_.empty()) return name_ + "=" + value_;
        return name_;
    }

    bool NameValueHeaderValue::TryParse(const std::string& input, NameValueHeaderValue& parsedValue) {
        size_t start = input.find_first_not_of(" \t");
        if (start == std::string::npos) return false;
        size_t end = input.find_last_not_of(" \t");
        std::string trimmed = input.substr(start, end - start + 1);

        size_t eq = trimmed.find('=');
        std::string name = (eq == std::string::npos) ? trimmed : trimmed.substr(0, eq);
        std::string value = (eq == std::string::npos) ? "" : trimmed.substr(eq + 1);

        size_t nameEnd = name.find_last_not_of(" \t");
        if (nameEnd == std::string::npos) return false;
        name = name.substr(0, nameEnd + 1);

        if (eq != std::string::npos) {
            size_t valueStart = value.find_first_not_of(" \t");
            value = (valueStart == std::string::npos) ? "" : value.substr(valueStart);
        }

        if (!isToken(name)) return false;
        if (!value.empty()) {
            try {
                checkValueFormat(value);
            } catch (...) {
                return false;
            }
        }

        NameValueHeaderValue result;
        result.name_ = name;
        result.value_ = value;
        parsedValue = result;
        return true;
    }

    NameValueHeaderValue NameValueHeaderValue::Parse(const std::string& input) {
        NameValueHeaderValue result;
        if (!TryParse(input, result)) {
            throw System::FormatException("The header value is not valid: " + input);
        }
        return result;
    }

} // namespace System::Net::Http::Headers
