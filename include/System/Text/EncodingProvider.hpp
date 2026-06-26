// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <memory>
#include <string>
#include "System/Text/Encoding.hpp"

namespace System::Text {

    /** Abstract base for custom encoding providers. */
    class EncodingProvider {
    public:
        virtual ~EncodingProvider() = default;
        /** Returns an encoding for the given code-page identifier. */
        [[nodiscard]] virtual std::shared_ptr<Encoding> GetEncoding(int codepage) = 0;
        /** Returns an encoding for the given encoding name. */
        [[nodiscard]] virtual std::shared_ptr<Encoding> GetEncoding(const std::string& name) = 0;
    };

} // namespace System::Text
