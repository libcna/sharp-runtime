// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System {

    // .NET 7+ interface; C++ version is a simplified approximation
    class ISpanFormattable {
    public:
        virtual ~ISpanFormattable() = default;
        virtual std::string ToString(const std::string& format, const void* formatProvider = nullptr) const = 0;
    };

} // namespace System
