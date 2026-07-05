// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/Stream.hpp"
#include "System/ArgumentException.hpp"
#include "System/NotSupportedException.hpp"

namespace System::IO {

    void Stream::Write(const bytecs[], intcs, intcs) {
        throw System::NotSupportedException("Stream does not support writing.");
    }

    void Stream::WriteByte(bytecs value) {
        Write(&value, 0, 1);
    }

    intcs Stream::getPositionProperty() const {
        throw System::NotSupportedException("Stream does not support seeking.");
    }

    void Stream::setPositionProperty(intcs) {
        throw System::NotSupportedException("Stream does not support seeking.");
    }

    intcs Stream::Seek(intcs offset, SeekOrigin origin) {
        intcs newPosition;
        switch (origin) {
            case SeekOrigin::Begin:
                newPosition = offset;
                break;
            case SeekOrigin::Current:
                newPosition = getPositionProperty() + offset;
                break;
            case SeekOrigin::End:
                newPosition = getLengthProperty() + offset;
                break;
            default:
                throw System::ArgumentException("Invalid seek origin.", "origin");
        }
        setPositionProperty(newPosition);
        return newPosition;
    }

    void Stream::SetLength(intcs) {
        throw System::NotSupportedException("Stream does not support SetLength.");
    }

    intcs Stream::ReadByte() {
        bytecs b = 0;
        intcs bytesRead = Read(&b, 0, 1);
        return bytesRead == 0 ? -1 : static_cast<intcs>(b);
    }

} // namespace System::IO
