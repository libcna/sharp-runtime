// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/UnmanagedMemoryStream.hpp"
#include "System/ArgumentNullException.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/NotSupportedException.hpp"
#include "System/IO/IOException.hpp"

#include <algorithm>
#include <cstring>

namespace System::IO {

    UnmanagedMemoryStream::UnmanagedMemoryStream(bytecs* pointer, intcs length)
    {
        Initialize(pointer, length, length, FileAccess::Read);
    }

    UnmanagedMemoryStream::UnmanagedMemoryStream(bytecs* pointer, intcs length, intcs capacity, FileAccess access)
    {
        Initialize(pointer, length, capacity, access);
    }

    void UnmanagedMemoryStream::Initialize(bytecs* pointer, intcs length, intcs capacity, FileAccess access)
    {
        if (pointer == nullptr) throw System::ArgumentNullException("pointer");
        if (length < 0) throw System::ArgumentOutOfRangeException("length", "Non-negative number required.");
        if (capacity < 0) throw System::ArgumentOutOfRangeException("capacity", "Non-negative number required.");
        if (length > capacity) throw System::ArgumentOutOfRangeException("length", "The length cannot be greater than the capacity.");

        buffer_   = pointer;
        length_   = length;
        capacity_ = capacity;
        access_   = access;
        position_ = 0;
        isOpen_   = true;
    }

    intcs UnmanagedMemoryStream::Read(bytecs buffer[], intcs offset, intcs count)
    {
        if (!getCanReadProperty()) throw System::NotSupportedException("Stream does not support reading.");
        if (buffer == nullptr || offset < 0 || count < 0) return 0;

        const intcs remaining = length_ - position_;
        const intcs toRead = std::min(count, remaining);
        if (toRead <= 0) return 0;

        std::memcpy(buffer + offset, buffer_ + position_, static_cast<size_t>(toRead));
        position_ += toRead;
        return toRead;
    }

    void UnmanagedMemoryStream::Write(const bytecs buffer[], intcs offset, intcs count)
    {
        if (!getCanWriteProperty()) throw System::NotSupportedException("Stream does not support writing.");
        if (buffer == nullptr || count <= 0) return;
        if (position_ + count > capacity_) {
            throw IOException("Unable to expand length of this stream beyond its capacity.");
        }

        std::memcpy(buffer_ + position_, buffer + offset, static_cast<size_t>(count));
        position_ += count;
        if (position_ > length_) length_ = position_;
    }

    void UnmanagedMemoryStream::WriteByte(bytecs value)
    {
        Write(&value, 0, 1);
    }

    void UnmanagedMemoryStream::setPositionProperty(intcs value)
    {
        if (value < 0) throw System::ArgumentOutOfRangeException("value", "Non-negative number required.");
        position_ = value;
    }

    void UnmanagedMemoryStream::SetLength(intcs value)
    {
        if (value < 0) throw System::ArgumentOutOfRangeException("value", "Non-negative number required.");
        if (!getCanWriteProperty()) throw System::NotSupportedException("Stream does not support writing.");
        if (value > capacity_) {
            throw IOException("Unable to expand length of this stream beyond its capacity.");
        }
        length_ = value;
        if (position_ > value) position_ = value;
    }

} // namespace System::IO
