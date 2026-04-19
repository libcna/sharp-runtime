#include "System/String.hpp"

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

    std::string String::Format(const std::string& format, CppDotNet::intcs arg0)
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
    std::string String::ToString(CppDotNet::intcs value, int width, char fill)
    {
        std::ostringstream oss;
        oss << std::setw(width) << std::setfill(fill) << value;
        return oss.str();
    }

}
