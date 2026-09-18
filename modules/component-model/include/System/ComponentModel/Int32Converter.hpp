// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Int32.hpp"
#include "System/ComponentModel/BaseNumberConverter.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a type converter to convert 32-bit signed integer objects to and from other representations.
     *
     * C++ counterpart of .NET System.ComponentModel.Int32Converter. Parses with
     * `System::Int32::Parse(text, NumberStyles::Integer, culture)` (a `#` or `0x` prefix selects hexadecimal) and formats with
     * `System::Int32::ToString(value, culture)`, so the culture's separators and signs are
     * honoured in both directions.
     */
    class Int32Converter : public detail::NumberConverterBase<SharpRuntime::intcs, System::Int32, true> {
    public:
        /** @brief Initializes a new instance of the Int32Converter class. */
        Int32Converter() = default;
    };

} // namespace System::ComponentModel
