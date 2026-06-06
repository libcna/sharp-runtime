// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Text/StringBuilder.hpp"

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

    StringBuilder& StringBuilder::Append(intcs value)
    {
        buffer += std::to_string(value);
        return *this;
    }

    StringBuilder& StringBuilder::Append(double value)
    {
        std::ostringstream oss;
        oss << value;
        buffer += oss.str();
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

    bool StringBuilder::Empty() const
    {
        return buffer.empty();
    }
}