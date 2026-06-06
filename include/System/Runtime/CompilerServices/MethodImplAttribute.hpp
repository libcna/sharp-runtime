// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "System/Attribute.hpp"
#include "System/Runtime/CompilerServices/MethodImplOptions.hpp"

namespace System::Runtime::CompilerServices {

    class MethodImplAttribute : public System::Attribute {
        MethodImplOptions value_;
    public:
        explicit MethodImplAttribute(MethodImplOptions value) : value_(value) {}
        explicit MethodImplAttribute(int16_t value) : value_(static_cast<MethodImplOptions>(value)) {}
        [[nodiscard]] MethodImplOptions getValueProperty() const { return value_; }
    };

} // namespace System::Runtime::CompilerServices
