#include "System/IO/StreamReader.hpp"

#include <vector>

namespace System::IO
{
    StreamReader::StreamReader(Stream* stream)
        : stream(stream)
    {
    }

    std::string StreamReader::ReadToEnd() const
    {
        if (stream == nullptr)
        {
            return "";
        }

        const intcs length = stream->getLengthProperty();
        if (length <= 0)
        {
            return "";
        }

        std::vector<char> buffer(static_cast<std::size_t>(length));
        const intcs bytesRead = stream->Read(reinterpret_cast<SharpRuntime::bytecs*>(buffer.data()), 0, length);

        return std::string(buffer.data(), static_cast<std::size_t>(bytesRead));
    }
}