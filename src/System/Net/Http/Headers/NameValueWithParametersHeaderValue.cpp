// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Net/Http/Headers/NameValueWithParametersHeaderValue.hpp"
#include "System/FormatException.hpp"
#include "System/HashCode.hpp"
#include <algorithm>

namespace System::Net::Http::Headers {

    namespace {
        std::string trim(const std::string& s) {
            size_t start = s.find_first_not_of(" \t");
            if (start == std::string::npos) return "";
            size_t end = s.find_last_not_of(" \t");
            return s.substr(start, end - start + 1);
        }
    }

    NameValueWithParametersHeaderValue::NameValueWithParametersHeaderValue(const std::string& name)
        : NameValueHeaderValue(name) {}

    NameValueWithParametersHeaderValue::NameValueWithParametersHeaderValue(const std::string& name, const std::string& value)
        : NameValueHeaderValue(name, value) {}

    bool NameValueWithParametersHeaderValue::Equals(const NameValueWithParametersHeaderValue& other) const {
        if (!NameValueHeaderValue::Equals(other)) return false;
        if (parameters_.size() != other.parameters_.size()) return false;

        // Order-independent comparison, matching .NET's HeaderUtilities.AreEqualCollections.
        std::vector<bool> matched(other.parameters_.size(), false);
        for (const auto& p : parameters_) {
            bool found = false;
            for (size_t i = 0; i < other.parameters_.size(); ++i) {
                if (!matched[i] && p == other.parameters_[i]) {
                    matched[i] = true;
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        return true;
    }

    SharpRuntime::intcs NameValueWithParametersHeaderValue::GetHashCode() const {
        SharpRuntime::intcs result = NameValueHeaderValue::GetHashCode();
        for (const auto& p : parameters_) result ^= p.GetHashCode();
        return result;
    }

    std::string NameValueWithParametersHeaderValue::ToString() const {
        std::string result = NameValueHeaderValue::ToString();
        for (const auto& p : parameters_) {
            result += "; " + p.ToString();
        }
        return result;
    }

    bool NameValueWithParametersHeaderValue::TryParse(const std::string& input, NameValueWithParametersHeaderValue& parsedValue) {
        size_t semi = input.find(';');
        std::string head = trim(semi == std::string::npos ? input : input.substr(0, semi));
        if (head.empty()) return false;

        size_t eq = head.find('=');
        std::string name = trim(eq == std::string::npos ? head : head.substr(0, eq));
        std::string value = eq == std::string::npos ? "" : trim(head.substr(eq + 1));

        try {
            NameValueWithParametersHeaderValue result = value.empty()
                ? NameValueWithParametersHeaderValue(name)
                : NameValueWithParametersHeaderValue(name, value);

            if (semi != std::string::npos) {
                std::string rest = input.substr(semi + 1);
                size_t start = 0;
                while (start <= rest.size()) {
                    size_t next = rest.find(';', start);
                    std::string segment = trim(next == std::string::npos ? rest.substr(start) : rest.substr(start, next - start));
                    if (segment.empty()) return false;

                    NameValueHeaderValue param("x");
                    if (!NameValueHeaderValue::TryParse(segment, param)) return false;
                    result.parameters_.push_back(param);

                    if (next == std::string::npos) break;
                    start = next + 1;
                }
            }

            parsedValue = result;
            return true;
        } catch (...) {
            return false;
        }
    }

    NameValueWithParametersHeaderValue NameValueWithParametersHeaderValue::Parse(const std::string& input) {
        NameValueWithParametersHeaderValue result("x");
        if (!TryParse(input, result)) {
            throw System::FormatException("The header value is not valid: " + input);
        }
        return result;
    }

} // namespace System::Net::Http::Headers
