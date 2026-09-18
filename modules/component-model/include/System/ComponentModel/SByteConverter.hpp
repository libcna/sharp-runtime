// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/SByte.hpp"
#include "System/ComponentModel/BaseNumberConverter.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a type converter to convert 8-bit signed integer objects to and from a string.
     *
     * C++ counterpart of .NET System.ComponentModel.SByteConverter. Parses with
     * `System::SByte::Parse(text, NumberStyles::Integer, culture)` (a `#` or `0x` prefix selects hexadecimal) and formats with
     * `System::SByte::ToString(value, culture)`, so the culture's separators and signs are
     * honoured in both directions.
     */
    class SByteConverter : public detail::NumberConverterBase<SharpRuntime::sbytecs, System::SByte, true> {
    public:
        /** @brief Initializes a new instance of the SByteConverter class. */
        SByteConverter() = default;
    };

} // namespace System::ComponentModel
