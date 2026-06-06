// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/MemoryStream.hpp"
#include <algorithm>

namespace System::IO
{
    MemoryStream::MemoryStream(const bytecs* buffer, intcs size)
        : data(buffer, buffer + size),
          position(0)
    {
    }

    intcs MemoryStream::Read(bytecs buffer[], intcs offset, intcs count)
    {
        if (buffer == nullptr || offset < 0 || count < 0)
        {
            return 0;
        }
        const intcs remaining = static_cast<intcs>(data.size()) - position;
        const intcs toRead = std::min(count, remaining);
        if (toRead <= 0)
        {
            return 0;
        }
        std::copy(data.begin() + position, data.begin() + position + toRead, buffer + offset);
        position += toRead;
        return toRead;
    }

    void MemoryStream::Close()
    {
        data.clear();
        position = 0;
    }

    intcs MemoryStream::getLengthProperty() const
    {
        return static_cast<intcs>(data.size());
    }
}
