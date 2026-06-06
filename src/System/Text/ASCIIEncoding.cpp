// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/Text/ASCIIEncoding.hpp"

namespace System::Text {

    std::vector<SharpRuntime::bytecs> ASCIIEncoding::GetBytes(const std::string& str) const {
        std::vector<SharpRuntime::bytecs> result;
        result.reserve(str.size());
        for (unsigned char c : str)
            result.push_back(c <= 127 ? static_cast<SharpRuntime::bytecs>(c) : '?');
        return result;
    }

    std::string ASCIIEncoding::GetString(const SharpRuntime::bytecs* data,
                                         SharpRuntime::intcs index,
                                         SharpRuntime::intcs count) const {
        if (data == nullptr || count <= 0) return {};
        std::string result;
        result.reserve(static_cast<size_t>(count));
        for (SharpRuntime::intcs i = 0; i < count; ++i) {
            auto b = data[index + i];
            result.push_back(b <= 127 ? static_cast<char>(b) : '?');
        }
        return result;
    }

} // namespace System::Text
