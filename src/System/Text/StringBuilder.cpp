// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Text/StringBuilder.hpp"
#include "System/ArgumentException.hpp"

#include <array>
#include <charconv>
#include <sstream>
#include <utility>

namespace System::Text
{
    StringBuilder::StringBuilder()
        : buffer()
    {
    }

    StringBuilder::StringBuilder(const std::string& value)
        : buffer(value)
    {
    }

    void StringBuilder::Clear()
    {
        buffer.clear();
    }

    StringBuilder& StringBuilder::Append(const std::string& value)
    {
        buffer += value;
        return *this;
    }

    StringBuilder& StringBuilder::Append(const char* value)
    {
        if (value != nullptr)
        {
            buffer += value;
        }
        return *this;
    }

    StringBuilder& StringBuilder::Append(char value)
    {
        buffer += value;
        return *this;
    }

    StringBuilder& StringBuilder::Append(char value, intcs repeatCount)
    {
        buffer.append(static_cast<size_t>(repeatCount), value);
        return *this;
    }

    StringBuilder& StringBuilder::Append(intcs value)
    {
        buffer += std::to_string(value);
        return *this;
    }

    StringBuilder& StringBuilder::Append(double value)
    {
        std::array<char, 64> buf;
        auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
        buffer += (ec == std::errc{}) ? std::string(buf.data(), ptr) : std::to_string(value);
        return *this;
    }

    StringBuilder& StringBuilder::Append(float value)
    {
        std::array<char, 32> buf;
        auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), value);
        buffer += (ec == std::errc{}) ? std::string(buf.data(), ptr) : std::to_string(value);
        return *this;
    }

    StringBuilder& StringBuilder::Append(bool value)
    {
        buffer += (value ? "True" : "False");
        return *this;
    }

    StringBuilder& StringBuilder::AppendLine()
    {
        buffer += '\n';
        return *this;
    }

    StringBuilder& StringBuilder::AppendLine(const std::string& value)
    {
        buffer += value;
        buffer += '\n';
        return *this;
    }

    std::string StringBuilder::ToString() const
    {
        return buffer;
    }

    intcs StringBuilder::getLengthProperty() const
    {
        return static_cast<intcs>(buffer.size());
    }

    void StringBuilder::setLengthProperty(intcs value)
    {
        buffer.resize(static_cast<size_t>(value), '\0');
    }

    bool StringBuilder::Empty() const
    {
        return buffer.empty();
    }

    StringBuilder& StringBuilder::Append(SharpRuntime::longcs value)
    {
        buffer += std::to_string(value);
        return *this;
    }

    StringBuilder& StringBuilder::Insert(intcs index, const std::string& value)
    {
        buffer.insert(static_cast<size_t>(index), value);
        return *this;
    }

    StringBuilder& StringBuilder::Remove(intcs startIndex, intcs count)
    {
        buffer.erase(static_cast<size_t>(startIndex), static_cast<size_t>(count));
        return *this;
    }

    StringBuilder& StringBuilder::Replace(const std::string& oldValue, const std::string& newValue)
    {
        // .NET throws for a null/empty oldValue (ArgumentException.ThrowIfNullOrEmpty,
        // StringBuilder.cs) rather than looping. Without this check, buffer.find("", pos)
        // always succeeds at pos: if newValue is also empty, pos never advances and this
        // loops forever; if newValue is non-empty, each iteration inserts newValue.size()
        // characters and advances pos by exactly that much, so buffer.size() - pos never
        // shrinks and the loop still never terminates (runs until OOM).
        if (oldValue.empty())
            throw System::ArgumentException("The value cannot be an empty string.", "oldValue");
        size_t pos = 0;
        while ((pos = buffer.find(oldValue, pos)) != std::string::npos) {
            buffer.replace(pos, oldValue.size(), newValue);
            pos += newValue.size();
        }
        return *this;
    }
}