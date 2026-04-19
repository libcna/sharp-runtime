#include "System/Text/Encoding.hpp"

namespace System::Text
{
    std::shared_ptr<Encoding> Encoding::UTF8()
    {
        static std::shared_ptr<Encoding> instance = std::shared_ptr<Encoding>(new Encoding());
        return instance;
    }

    std::vector<CppDotNet::bytecs> Encoding::GetBytes(const std::string& str) const
    {
        return std::vector<CppDotNet::bytecs>(str.begin(), str.end());
    }

    std::string Encoding::GetString(
        const CppDotNet::bytecs* data,
        CppDotNet::intcs index,
        CppDotNet::intcs count) const
    {
        if (data == nullptr || count <= 0)
        {
            return {};
        }

        return std::string(
            reinterpret_cast<const char*>(data + index),
            static_cast<size_t>(count));
    }
}