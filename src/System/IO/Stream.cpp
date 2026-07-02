// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/IO/Stream.hpp"
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

} // namespace System::IO
