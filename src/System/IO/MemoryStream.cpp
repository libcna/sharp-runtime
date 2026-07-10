// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/MemoryStream.hpp"
#include "System/ArgumentOutOfRangeException.hpp"
#include "System/NotSupportedException.hpp"
#include <algorithm>

namespace System::IO
{
    MemoryStream::MemoryStream()
        : position_(0), writable_(true) {}

    MemoryStream::MemoryStream(const bytecs* buffer, intcs size)
        : data_(buffer, buffer + size), position_(0), writable_(false) {}

    intcs MemoryStream::Read(bytecs buffer[], intcs offset, intcs count)
    {
        if (buffer == nullptr || offset < 0 || count < 0) return 0;
        const intcs remaining = static_cast<intcs>(data_.size()) - position_;
        const intcs toRead = std::min(count, remaining);
        if (toRead <= 0) return 0;
        std::copy(data_.begin() + position_, data_.begin() + position_ + toRead, buffer + offset);
        position_ += toRead;
        return toRead;
    }

    void MemoryStream::Write(const bytecs buffer[], intcs offset, intcs count)
    {
        // Verified against MemoryStream.cs's Write()/EnsureWriteable(): real .NET throws
        // NotSupportedException when !CanWrite, matching this class's own SetLength(). This
        // port previously just returned, silently dropping every byte the caller thought it
        // had written.
        if (!writable_) throw System::NotSupportedException("Stream does not support writing.");
        if (buffer == nullptr || count <= 0) return;
        if (position_ + count > static_cast<intcs>(data_.size()))
            data_.resize(static_cast<size_t>(position_ + count));
        std::copy(buffer + offset, buffer + offset + count, data_.begin() + position_);
        position_ += count;
    }

    void MemoryStream::WriteByte(bytecs value)
    {
        if (!writable_) throw System::NotSupportedException("Stream does not support writing.");
        if (position_ >= static_cast<intcs>(data_.size())) data_.push_back(value);
        else data_[static_cast<size_t>(position_)] = value;
        ++position_;
    }

    void MemoryStream::Close()
    {
        data_.clear();
        position_ = 0;
    }

    intcs MemoryStream::getLengthProperty() const
    {
        return static_cast<intcs>(data_.size());
    }

    void MemoryStream::setPositionProperty(intcs value)
    {
        if (value < 0)
            throw System::ArgumentOutOfRangeException("value", "Non-negative number required.");
        position_ = value;
    }

    void MemoryStream::SetLength(intcs value)
    {
        if (value < 0)
            throw System::ArgumentOutOfRangeException("value", "Non-negative number required.");
        if (!writable_)
            throw System::NotSupportedException("Stream does not support writing.");
        data_.resize(static_cast<size_t>(value), bytecs{0});
        if (position_ > value) position_ = value;
    }
}
