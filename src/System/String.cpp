// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/String.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace System
{
    std::vector<std::string> String::Split(const std::string& value, char delimiter)
    {
        std::vector<std::string> result;
        std::stringstream ss(value);
        std::string item;

        while (std::getline(ss, item, delimiter))
        {
            result.push_back(item);
        }

        if (!value.empty() && value.back() == delimiter)
        {
            result.emplace_back();
        }

        return result;
    }

    std::vector<std::string> String::Split(const std::string& value, const std::vector<char>& delimiters)
    {
        std::vector<std::string> result;
        std::string current;
        for (char c : value) {
            bool isDelim = false;
            for (char d : delimiters) if (c == d) { isDelim = true; break; }
            if (isDelim) { result.push_back(current); current.clear(); }
            else current += c;
        }
        result.push_back(current);
        return result;
    }

    std::vector<std::string> String::Split(const std::string& value, const std::string& delimiter)
    {
        std::vector<std::string> result;
        if (delimiter.empty()) { result.push_back(value); return result; }
        size_t pos = 0, found;
        while ((found = value.find(delimiter, pos)) != std::string::npos) {
            result.push_back(value.substr(pos, found - pos));
            pos = found + delimiter.size();
        }
        result.push_back(value.substr(pos));
        return result;
    }

    bool String::IsEmpty(const std::string& value)
    {
        return value.empty();
    }

    bool String::StartsWith(const std::string& value, const std::string& prefix)
    {
        return value.size() >= prefix.size() &&
               value.compare(0, prefix.size(), prefix) == 0;
    }

    bool String::IsNullOrEmpty(const std::string& value)
    {
        return value.empty();
    }

    // --- Format helpers (file-internal) ---
    namespace {
        // Extract the spec from the first occurrence of {N:spec} in fmt (returns "" if none).
        std::string extractSpec(const std::string& fmt, int n) {
            std::string tok = "{" + std::to_string(n);
            size_t pos = fmt.find(tok);
            if (pos == std::string::npos) return "";
            size_t after = pos + tok.size();
            if (after >= fmt.size() || fmt[after] != ':') return "";
            size_t end = fmt.find('}', after);
            return end == std::string::npos ? "" : fmt.substr(after + 1, end - after - 1);
        }

        // Replace every {N} or {N:spec} occurrence in result with value.
        std::string replaceArg(const std::string& fmt, int n, const std::string& value) {
            std::string result = fmt;
            std::string tok = "{" + std::to_string(n);
            size_t pos;
            while ((pos = result.find(tok)) != std::string::npos) {
                size_t end = result.find('}', pos);
                if (end == std::string::npos) break;
                result.replace(pos, end - pos + 1, value);
            }
            return result;
        }

        // Format integer with .NET-style specifier (X/x=hex, D=decimal padded, else plain).
        std::string fmtInt(SharpRuntime::intcs value, const std::string& spec) {
            if (spec.empty()) return std::to_string(value);
            char sc = spec[0];
            int width = spec.size() > 1 ? std::abs(std::stoi(spec.substr(1))) : 0;
            if (sc == 'X' || sc == 'x') {
                std::ostringstream oss;
                if (sc == 'X') oss << std::uppercase;
                oss << std::hex;
                if (width > 0) oss << std::setw(width) << std::setfill('0');
                oss << static_cast<unsigned int>(value);
                return oss.str();
            }
            if (sc == 'D' || sc == 'd') {
                bool neg = value < 0;
                std::string s = std::to_string(neg ? -static_cast<long long>(value) : static_cast<long long>(value));
                while (static_cast<int>(s.size()) < width) s = "0" + s;
                return neg ? "-" + s : s;
            }
            return std::to_string(value);
        }

        // Format double with .NET-style specifier (F=fixed, G=general, E=scientific).
        std::string fmtDouble(double value, const std::string& spec) {
            if (spec.empty()) {
                std::array<char, 64> buf;
                auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
                return ec == std::errc{} ? std::string(buf.data(), ptr) : std::to_string(value);
            }
            char sc = spec[0];
            int prec = spec.size() > 1 ? std::stoi(spec.substr(1)) : 2;
            std::ostringstream oss;
            if (sc == 'F' || sc == 'f') {
                oss << std::fixed << std::setprecision(prec) << value;
            } else if (sc == 'G' || sc == 'g') {
                oss << std::defaultfloat << std::setprecision(prec > 0 ? prec : 6) << value;
            } else if (sc == 'E' || sc == 'e') {
                if (sc == 'E') oss << std::uppercase;
                oss << std::scientific << std::setprecision(prec) << value;
            } else {
                oss << value;
            }
            return oss.str();
        }
    }

    std::string String::Format(const std::string& format, SharpRuntime::intcs arg0)
    {
        return replaceArg(format, 0, fmtInt(arg0, extractSpec(format, 0)));
    }

    std::string String::Format(const std::string& format, double arg0)
    {
        return replaceArg(format, 0, fmtDouble(arg0, extractSpec(format, 0)));
    }

    std::string String::Format(const std::string& format, const std::string& arg0)
    {
        return replaceArg(format, 0, arg0);
    }

    std::string String::Format(const std::string& format, SharpRuntime::intcs arg0, SharpRuntime::intcs arg1)
    {
        std::string r = replaceArg(format,  0, fmtInt(arg0, extractSpec(format, 0)));
        return         replaceArg(r, 1, fmtInt(arg1, extractSpec(format, 1)));
    }

    std::string String::Format(const std::string& format, SharpRuntime::intcs arg0, const std::string& arg1)
    {
        std::string r = replaceArg(format, 0, fmtInt(arg0, extractSpec(format, 0)));
        return         replaceArg(r, 1, arg1);
    }

    std::string String::Format(const std::string& format, const std::string& arg0, SharpRuntime::intcs arg1)
    {
        std::string r = replaceArg(format, 0, arg0);
        return         replaceArg(r, 1, fmtInt(arg1, extractSpec(format, 1)));
    }

    std::string String::Format(const std::string& format, const std::string& arg0, const std::string& arg1)
    {
        std::string r = replaceArg(format, 0, arg0);
        return         replaceArg(r, 1, arg1);
    }

    std::string String::Format(const std::string& format, double arg0, double arg1)
    {
        std::string r = replaceArg(format, 0, fmtDouble(arg0, extractSpec(format, 0)));
        return         replaceArg(r, 1, fmtDouble(arg1, extractSpec(format, 1)));
    }
    std::string String::Format(const std::string& format, bool arg0)
    {
        return Format(format, arg0 ? std::string("True") : std::string("False"));
    }
    std::string String::Format(const std::string& format, char arg0)
    {
        return Format(format, std::string(1, arg0));
    }
    std::string String::ToString(SharpRuntime::intcs value, int width, char fill)
    {
        std::ostringstream oss;
        oss << std::setw(width) << std::setfill(fill) << value;
        return oss.str();
    }

    bool String::IsNullOrWhiteSpace(const std::string& value)
    {
        for (char c : value)
            if (!std::isspace(static_cast<unsigned char>(c))) return false;
        return true;
    }

    bool String::EndsWith(const std::string& value, const std::string& suffix)
    {
        if (suffix.size() > value.size()) return false;
        return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    bool String::Contains(const std::string& value, const std::string& substr)
    {
        return value.find(substr) != std::string::npos;
    }

    std::string String::Replace(const std::string& value, const std::string& oldValue, const std::string& newValue)
    {
        if (oldValue.empty()) return value;
        std::string result;
        result.reserve(value.size());
        size_t pos = 0, found;
        while ((found = value.find(oldValue, pos)) != std::string::npos)
        {
            result.append(value, pos, found - pos);
            result.append(newValue);
            pos = found + oldValue.size();
        }
        result.append(value, pos, std::string::npos);
        return result;
    }

    std::string String::Replace(const std::string& value, char oldChar, char newChar)
    {
        std::string result = value;
        for (char& c : result)
            if (c == oldChar) c = newChar;
        return result;
    }

    std::string String::Substring(const std::string& value, SharpRuntime::intcs startIndex)
    {
        return value.substr(static_cast<size_t>(startIndex));
    }

    std::string String::Substring(const std::string& value, SharpRuntime::intcs startIndex, SharpRuntime::intcs length)
    {
        return value.substr(static_cast<size_t>(startIndex), static_cast<size_t>(length));
    }

    std::string String::Trim(const std::string& value)
    {
        auto first = value.find_first_not_of(" \t\n\r\f\v");
        if (first == std::string::npos) return {};
        auto last = value.find_last_not_of(" \t\n\r\f\v");
        return value.substr(first, last - first + 1);
    }

    std::string String::TrimStart(const std::string& value)
    {
        auto first = value.find_first_not_of(" \t\n\r\f\v");
        return first == std::string::npos ? std::string{} : value.substr(first);
    }

    std::string String::TrimEnd(const std::string& value)
    {
        auto last = value.find_last_not_of(" \t\n\r\f\v");
        return last == std::string::npos ? std::string{} : value.substr(0, last + 1);
    }

    std::string String::Concat(const std::string& a, const std::string& b)
    {
        return a + b;
    }

    std::string String::Concat(const std::string& a, const std::string& b, const std::string& c)
    {
        return a + b + c;
    }

    std::string String::Concat(const std::string& a, const std::string& b, const std::string& c, const std::string& d)
    {
        return a + b + c + d;
    }

    std::string String::Concat(const std::vector<std::string>& values)
    {
        std::string result;
        for (const auto& s : values) result += s;
        return result;
    }

    std::string String::Join(const std::string& separator, const std::vector<std::string>& values)
    {
        std::string result;
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0) result += separator;
            result += values[i];
        }
        return result;
    }

    std::string String::ToUpper(const std::string& value)
    {
        std::string result = value;
        for (char& c : result) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return result;
    }

    std::string String::ToLower(const std::string& value)
    {
        std::string result = value;
        for (char& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return result;
    }

    SharpRuntime::intcs String::IndexOf(const std::string& value, const std::string& substr)
    {
        auto pos = value.find(substr);
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    SharpRuntime::intcs String::IndexOf(const std::string& value, char ch)
    {
        auto pos = value.find(ch);
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    SharpRuntime::intcs String::LastIndexOf(const std::string& value, const std::string& substr)
    {
        auto pos = value.rfind(substr);
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    SharpRuntime::intcs String::LastIndexOf(const std::string& value, char ch)
    {
        auto pos = value.rfind(ch);
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    std::string String::PadLeft(const std::string& value, SharpRuntime::intcs totalWidth)
    {
        return PadLeft(value, totalWidth, ' ');
    }

    std::string String::PadLeft(const std::string& value, SharpRuntime::intcs totalWidth, char paddingChar)
    {
        if (static_cast<SharpRuntime::intcs>(value.size()) >= totalWidth) return value;
        return std::string(static_cast<size_t>(totalWidth) - value.size(), paddingChar) + value;
    }

    std::string String::PadRight(const std::string& value, SharpRuntime::intcs totalWidth)
    {
        return PadRight(value, totalWidth, ' ');
    }

    std::string String::PadRight(const std::string& value, SharpRuntime::intcs totalWidth, char paddingChar)
    {
        if (static_cast<SharpRuntime::intcs>(value.size()) >= totalWidth) return value;
        return value + std::string(static_cast<size_t>(totalWidth) - value.size(), paddingChar);
    }

    SharpRuntime::intcs String::IndexOfAny(const std::string& value, const std::vector<char>& anyOf)
    {
        for (SharpRuntime::intcs i = 0; i < static_cast<SharpRuntime::intcs>(value.size()); ++i)
            for (char c : anyOf)
                if (value[static_cast<size_t>(i)] == c) return i;
        return -1;
    }

    SharpRuntime::intcs String::LastIndexOfAny(const std::string& value, const std::vector<char>& anyOf)
    {
        for (SharpRuntime::intcs i = static_cast<SharpRuntime::intcs>(value.size()) - 1; i >= 0; --i)
            for (char c : anyOf)
                if (value[static_cast<size_t>(i)] == c) return i;
        return -1;
    }

    bool String::Contains(const std::string& value, char ch)
    {
        return value.find(ch) != std::string::npos;
    }

    SharpRuntime::intcs String::IndexOf(const std::string& value, const std::string& substr, SharpRuntime::intcs startIndex)
    {
        auto pos = value.find(substr, static_cast<size_t>(startIndex));
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    SharpRuntime::intcs String::IndexOf(const std::string& value, char ch, SharpRuntime::intcs startIndex)
    {
        auto pos = value.find(ch, static_cast<size_t>(startIndex));
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    SharpRuntime::intcs String::LastIndexOf(const std::string& value, const std::string& substr, SharpRuntime::intcs startIndex)
    {
        auto pos = value.rfind(substr, static_cast<size_t>(startIndex));
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    SharpRuntime::intcs String::LastIndexOf(const std::string& value, char ch, SharpRuntime::intcs startIndex)
    {
        auto pos = value.rfind(ch, static_cast<size_t>(startIndex));
        return pos == std::string::npos ? -1 : static_cast<SharpRuntime::intcs>(pos);
    }

    std::string String::Create(SharpRuntime::intcs count, char ch)
    {
        return std::string(static_cast<size_t>(count), ch);
    }

    SharpRuntime::intcs String::Compare(const std::string& a, const std::string& b)
    {
        return static_cast<SharpRuntime::intcs>(a.compare(b));
    }

    SharpRuntime::intcs String::Compare(const std::string& a, const std::string& b, bool ignoreCase)
    {
        if (!ignoreCase) return Compare(a, b);
        std::string la = ToLower(a), lb = ToLower(b);
        return static_cast<SharpRuntime::intcs>(la.compare(lb));
    }

    bool String::Equals(const std::string& a, const std::string& b)
    {
        return a == b;
    }

    bool String::Equals(const std::string& a, const std::string& b, bool ignoreCase)
    {
        if (!ignoreCase) return a == b;
        return ToLower(a) == ToLower(b);
    }

    std::string String::Format(const std::string& format, SharpRuntime::intcs arg0, SharpRuntime::intcs arg1, SharpRuntime::intcs arg2)
    {
        std::string r = replaceArg(format, 0, fmtInt(arg0, extractSpec(format, 0)));
        r = replaceArg(r, 1, fmtInt(arg1, extractSpec(format, 1)));
        return replaceArg(r, 2, fmtInt(arg2, extractSpec(format, 2)));
    }

    std::string String::Format(const std::string& format, const std::string& arg0, const std::string& arg1, const std::string& arg2)
    {
        std::string r = replaceArg(format, 0, arg0);
        r = replaceArg(r, 1, arg1);
        return replaceArg(r, 2, arg2);
    }

    std::string String::Format(const std::string& format, SharpRuntime::longcs arg0)
    {
        return Format(format, std::to_string(arg0));
    }

    bool String::StartsWith(const std::string& value, char ch)
    {
        return !value.empty() && value.front() == ch;
    }

    bool String::EndsWith(const std::string& value, char ch)
    {
        return !value.empty() && value.back() == ch;
    }

    std::string String::Remove(const std::string& value, SharpRuntime::intcs startIndex)
    {
        return value.substr(0, static_cast<size_t>(startIndex));
    }

    std::string String::Remove(const std::string& value, SharpRuntime::intcs startIndex, SharpRuntime::intcs count)
    {
        std::string result = value;
        result.erase(static_cast<size_t>(startIndex), static_cast<size_t>(count));
        return result;
    }

    std::string String::Insert(const std::string& value, SharpRuntime::intcs startIndex, const std::string& insertValue)
    {
        std::string result = value;
        result.insert(static_cast<size_t>(startIndex), insertValue);
        return result;
    }

    std::string String::Join(const std::string& separator, const std::vector<SharpRuntime::intcs>& values)
    {
        std::string result;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) result += separator;
            result += std::to_string(values[i]);
        }
        return result;
    }

    std::string String::Join(const std::string& separator, const std::vector<double>& values)
    {
        std::string result;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) result += separator;
            result += std::to_string(values[i]);
        }
        return result;
    }

    std::vector<char> String::ToCharArray(const std::string& value)
    {
        return std::vector<char>(value.begin(), value.end());
    }

    std::string String::Trim(const std::string& value, const std::vector<char>& trimChars)
    {
        return TrimEnd(TrimStart(value, trimChars), trimChars);
    }

    std::string String::TrimStart(const std::string& value, const std::vector<char>& trimChars)
    {
        size_t start = 0;
        while (start < value.size()) {
            bool found = false;
            for (char c : trimChars) if (value[start] == c) { found = true; break; }
            if (!found) break;
            ++start;
        }
        return value.substr(start);
    }

    std::string String::TrimEnd(const std::string& value, const std::vector<char>& trimChars)
    {
        size_t end = value.size();
        while (end > 0) {
            bool found = false;
            for (char c : trimChars) if (value[end - 1] == c) { found = true; break; }
            if (!found) break;
            --end;
        }
        return value.substr(0, end);
    }

    static std::vector<std::string> applySplitOptions(std::vector<std::string> parts, StringSplitOptions options)
    {
        bool removeEmpty = (static_cast<int>(options) & static_cast<int>(StringSplitOptions::RemoveEmptyEntries)) != 0;
        bool trimEntries = (static_cast<int>(options) & static_cast<int>(StringSplitOptions::TrimEntries)) != 0;
        std::vector<std::string> result;
        for (auto& p : parts) {
            if (trimEntries) p = String::Trim(p);
            if (removeEmpty && p.empty()) continue;
            result.push_back(std::move(p));
        }
        return result;
    }

    std::vector<std::string> String::Split(const std::string& value, char delimiter, StringSplitOptions options)
    {
        return applySplitOptions(Split(value, delimiter), options);
    }

    std::vector<std::string> String::Split(const std::string& value, const std::string& delimiter, StringSplitOptions options)
    {
        return applySplitOptions(Split(value, delimiter), options);
    }

}
