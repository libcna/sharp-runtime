// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string_view>
#include "System/Xml/XmlException.hpp"

namespace System::Xml::detail {
    // UTF-8 is this runtime's string representation. At a character-input boundary
    // U+FEFF is document content, not an encoding preamble to be consumed by tinyxml2.
    inline bool HasCharacterPreamble(std::string_view text) {
        return text.starts_with("\xEF\xBB\xBF");
    }

    [[noreturn]] inline void ThrowCharacterPreambleError() {
        throw XmlException("Data at the root level is invalid. Line 1, position 1.", 1, 1);
    }
}
