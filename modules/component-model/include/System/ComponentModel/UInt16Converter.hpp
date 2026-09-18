// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/UInt16.hpp"
#include "System/ComponentModel/BaseNumberConverter.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a type converter to convert 16-bit unsigned integer objects to and from other representations.
     *
     * C++ counterpart of .NET System.ComponentModel.UInt16Converter. Parses with
     * `System::UInt16::Parse(text, NumberStyles::Integer, culture)` (a `#` or `0x` prefix selects hexadecimal) and formats with
     * `System::UInt16::ToString(value, culture)`, so the culture's separators and signs are
     * honoured in both directions.
     */
    class UInt16Converter : public detail::NumberConverterBase<SharpRuntime::ushortcs, System::UInt16, true> {
    public:
        /** @brief Initializes a new instance of the UInt16Converter class. */
        UInt16Converter() = default;
    };

} // namespace System::ComponentModel
