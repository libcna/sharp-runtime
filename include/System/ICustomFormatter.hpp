// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include <memory>
#include "System/IFormatProvider.hpp"

namespace System {

    class ICustomFormatter {
    public:
        virtual ~ICustomFormatter() = default;
        [[nodiscard]] virtual std::string Format(
            const std::string& format,
            const void* arg,
            const IFormatProvider* formatProvider) const = 0;
    };

} // namespace System
