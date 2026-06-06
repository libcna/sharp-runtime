// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/BitConverter.hpp"
#include <sstream>
#include <iomanip>

namespace System {

    std::string BitConverter::ToString(const bytecs* value, intcs startIndex, intcs length) {
        std::ostringstream oss;
        for (intcs i = 0; i < length; ++i) {
            if (i > 0) oss << '-';
            oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<unsigned>(static_cast<unsigned char>(value[startIndex + i]));
        }
        return oss.str();
    }

    std::string BitConverter::ToString(const std::vector<bytecs>& value) {
        return ToString(value.data(), 0, static_cast<intcs>(value.size()));
    }

} // namespace System
