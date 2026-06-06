// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include "System/Text/Encoding.hpp"

namespace System::Text {

    class EncodingProvider {
    public:
        virtual ~EncodingProvider() = default;
        [[nodiscard]] virtual std::shared_ptr<Encoding> GetEncoding(int codepage) = 0;
        [[nodiscard]] virtual std::shared_ptr<Encoding> GetEncoding(const std::string& name) = 0;
    };

} // namespace System::Text
