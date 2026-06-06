// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/FileStream.hpp"

#include <stdexcept>

namespace System::IO
{
    FileStream::FileStream(const std::string& path)
        : file(path, std::ios::binary),
          length(0)
    {
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + path);
        }

        file.seekg(0, std::ios::end);
        length = static_cast<intcs>(file.tellg());
        file.seekg(0, std::ios::beg);
    }

    FileStream::~FileStream()
    {
        Close();
    }

    intcs FileStream::Read(bytecs buffer[], intcs offset, intcs count)
    {
        if (!file.is_open() || buffer == nullptr || offset < 0 || count < 0)
        {
            return 0;
        }

        file.read(reinterpret_cast<char*>(buffer + offset), count);
        return static_cast<intcs>(file.gcount());
    }

    void FileStream::Close()
    {
        if (file.is_open())
        {
            file.close();
        }
    }

    intcs FileStream::getLengthProperty() const
    {
        return length;
    }

    bool FileStream::IsOpen() const
    {
        return file.is_open();
    }
}