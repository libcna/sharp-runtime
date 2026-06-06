// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>

namespace System {

    // .NET 7+ interface; C++ version is a simplified approximation
    template<typename TSelf>
    class ISpanParsable {
    public:
        virtual ~ISpanParsable() = default;
        static TSelf Parse(const std::string& s, const void* provider = nullptr) = delete;
        static bool  TryParse(const std::string& s, const void* provider, TSelf& result) = delete;
    };

} // namespace System
