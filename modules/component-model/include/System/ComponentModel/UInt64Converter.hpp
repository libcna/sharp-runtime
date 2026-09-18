// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/UInt64.hpp"
#include "System/ComponentModel/BaseNumberConverter.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a type converter to convert 64-bit unsigned integer objects to and from various other representations.
     *
     * C++ counterpart of .NET System.ComponentModel.UInt64Converter. Parses with
     * `System::UInt64::Parse(text, NumberStyles::Integer, culture)` (a `#` or `0x` prefix selects hexadecimal) and formats with
     * `System::UInt64::ToString(value, culture)`, so the culture's separators and signs are
     * honoured in both directions.
     */
    class UInt64Converter : public detail::NumberConverterBase<SharpRuntime::ulongcs, System::UInt64, true> {
    public:
        /** @brief Initializes a new instance of the UInt64Converter class. */
        UInt64Converter() = default;
    };

} // namespace System::ComponentModel
