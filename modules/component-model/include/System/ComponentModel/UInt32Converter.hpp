// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/UInt32.hpp"
#include "System/ComponentModel/BaseNumberConverter.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a type converter to convert 32-bit unsigned integer objects to and from various other representations.
     *
     * C++ counterpart of .NET System.ComponentModel.UInt32Converter. Parses with
     * `System::UInt32::Parse(text, NumberStyles::Integer, culture)` (a `#` or `0x` prefix selects hexadecimal) and formats with
     * `System::UInt32::ToString(value, culture)`, so the culture's separators and signs are
     * honoured in both directions.
     */
    class UInt32Converter : public detail::NumberConverterBase<SharpRuntime::uintcs, System::UInt32, true> {
    public:
        /** @brief Initializes a new instance of the UInt32Converter class. */
        UInt32Converter() = default;
    };

} // namespace System::ComponentModel
