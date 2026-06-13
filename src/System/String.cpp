// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/String.hpp"

#include <cctype>
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

    std::string String::Format(const std::string& format, SharpRuntime::intcs arg0)
    {
        return Format(format, std::to_string(arg0));
    }

    std::string String::Format(const std::string& format, const std::string& arg0)
    {
        std::string result = format;

        const std::string placeholder = "{0}";

        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos)
        {
            result.replace(pos, placeholder.length(), arg0);
            pos += arg0.length();
        }

        return result;
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

}
