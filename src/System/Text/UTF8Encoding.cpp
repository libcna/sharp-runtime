// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Text/UTF8Encoding.hpp"

namespace System::Text {

    std::vector<SharpRuntime::bytecs> UTF8Encoding::GetBytes(const std::string& str) const {
        return std::vector<SharpRuntime::bytecs>(str.begin(), str.end());
    }

    std::string UTF8Encoding::GetString(const SharpRuntime::bytecs* data,
                                        SharpRuntime::intcs index,
                                        SharpRuntime::intcs count) const {
        if (data == nullptr || count <= 0) return {};
        return std::string(reinterpret_cast<const char*>(data + index),
                           static_cast<size_t>(count));
    }

} // namespace System::Text
