// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Int64.hpp"
#include "System/ComponentModel/BaseNumberConverter.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a type converter to convert 64-bit signed integer objects to and from various other representations.
     *
     * C++ counterpart of .NET System.ComponentModel.Int64Converter. Parses with
     * `System::Int64::Parse(text, NumberStyles::Integer, culture)` (a `#` or `0x` prefix selects hexadecimal) and formats with
     * `System::Int64::ToString(value, culture)`, so the culture's separators and signs are
     * honoured in both directions.
     */
    class Int64Converter : public detail::NumberConverterBase<SharpRuntime::longcs, System::Int64, true> {
    public:
        /** @brief Initializes a new instance of the Int64Converter class. */
        Int64Converter() = default;
    };

} // namespace System::ComponentModel
