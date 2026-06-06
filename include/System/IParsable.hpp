// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>

namespace System {

    class IFormatProvider;

    template<typename TSelf>
    class IParsable {
    public:
        virtual ~IParsable() = default;
        virtual TSelf Parse(const std::string& s, const IFormatProvider* provider) = 0;
        virtual bool TryParse(const std::string& s, const IFormatProvider* provider, TSelf& result) = 0;
    };

} // namespace System
