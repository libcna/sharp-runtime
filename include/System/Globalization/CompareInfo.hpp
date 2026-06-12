// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Globalization/CompareOptions.hpp"
#include "System/Globalization/SortKey.hpp"

namespace System::Globalization {

using SharpRuntime::charcs;
using SharpRuntime::intcs;

/// <summary>Implements a set of methods for culture-sensitive string comparisons.</summary>
class CompareInfo {
public:
    explicit CompareInfo(const std::string& name = "en-US") : _name(name) {}

    static CompareInfo GetCompareInfo(const std::string& name) { return CompareInfo(name); }
    static CompareInfo GetCompareInfo(intcs /*culture*/) { return CompareInfo("en-US"); }

    [[nodiscard]] const std::string& getNameProperty() const { return _name; }

    /// <summary>Compares two strings. Returns negative, zero, or positive.</summary>
    intcs Compare(const std::string& s1, const std::string& s2,
                  CompareOptions options = CompareOptions::None) const {
        if (hasFlag(options, CompareOptions::IgnoreCase))
            return compareIgnoreCase(s1, s2);
        if (s1 < s2) return -1;
        if (s1 > s2) return  1;
        return 0;
    }

    intcs Compare(const std::string& s1, intcs off1, intcs len1,
                  const std::string& s2, intcs off2, intcs len2,
                  CompareOptions options = CompareOptions::None) const {
        return Compare(s1.substr(static_cast<size_t>(off1), static_cast<size_t>(len1)),
                       s2.substr(static_cast<size_t>(off2), static_cast<size_t>(len2)), options);
    }

    bool IsPrefix(const std::string& source, const std::string& prefix,
                  CompareOptions options = CompareOptions::None) const {
        if (prefix.size() > source.size()) return false;
        return Compare(source, 0, static_cast<intcs>(prefix.size()),
                       prefix, 0, static_cast<intcs>(prefix.size()), options) == 0;
    }

    bool IsSuffix(const std::string& source, const std::string& suffix,
                  CompareOptions options = CompareOptions::None) const {
        if (suffix.size() > source.size()) return false;
        intcs off = static_cast<intcs>(source.size() - suffix.size());
        return Compare(source, off, static_cast<intcs>(suffix.size()),
                       suffix, 0, static_cast<intcs>(suffix.size()), options) == 0;
    }

    intcs IndexOf(const std::string& source, const std::string& value,
                  CompareOptions options = CompareOptions::None) const {
        if (hasFlag(options, CompareOptions::IgnoreCase)) {
            std::string sl = toLower(source), vl = toLower(value);
            auto pos = sl.find(vl);
            return pos == std::string::npos ? -1 : static_cast<intcs>(pos);
        }
        auto pos = source.find(value);
        return pos == std::string::npos ? -1 : static_cast<intcs>(pos);
    }

    intcs LastIndexOf(const std::string& source, const std::string& value,
                      CompareOptions options = CompareOptions::None) const {
        if (hasFlag(options, CompareOptions::IgnoreCase)) {
            std::string sl = toLower(source), vl = toLower(value);
            auto pos = sl.rfind(vl);
            return pos == std::string::npos ? -1 : static_cast<intcs>(pos);
        }
        auto pos = source.rfind(value);
        return pos == std::string::npos ? -1 : static_cast<intcs>(pos);
    }

    static bool IsSortable(charcs /*ch*/) { return true; }
    static bool IsSortable(const std::string& /*text*/) { return true; }

    /// <summary>Gets the SortKey for a string (byte-level key for this stub).</summary>
    SortKey GetSortKey(const std::string& source,
                       CompareOptions /*options*/ = CompareOptions::None) const {
        std::vector<bytecs> key(source.begin(), source.end());
        return SortKey(source, key);
    }

    intcs GetHashCode(const std::string& source, CompareOptions /*options*/) const {
        return static_cast<intcs>(std::hash<std::string>{}(source));
    }

    bool operator==(const CompareInfo& other) const { return _name == other._name; }
    std::string ToString() const { return "CompareInfo - " + _name; }

private:
    std::string _name;

    static bool hasFlag(CompareOptions options, CompareOptions flag) {
        return (static_cast<int>(options) & static_cast<int>(flag)) != 0;
    }

    static std::string toLower(const std::string& s) {
        std::string r = s;
        for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return r;
    }

    static intcs compareIgnoreCase(const std::string& a, const std::string& b) {
        std::string al = toLower(a), bl = toLower(b);
        if (al < bl) return -1;
        if (al > bl) return  1;
        return 0;
    }
};

} // namespace System::Globalization
