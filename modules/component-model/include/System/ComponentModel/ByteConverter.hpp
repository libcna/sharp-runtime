// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Byte.hpp"
#include "System/ComponentModel/BaseNumberConverter.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a type converter to convert 8-bit unsigned integer objects to and from various other representations.
     *
     * C++ counterpart of .NET System.ComponentModel.ByteConverter. Parses with
     * `System::Byte::Parse(text, NumberStyles::Integer, culture)` (a `#` or `0x` prefix selects hexadecimal) and formats with
     * `System::Byte::ToString(value, culture)`, so the culture's separators and signs are
     * honoured in both directions.
     */
    class ByteConverter : public detail::NumberConverterBase<SharpRuntime::bytecs, System::Byte, true> {
    public:
        /** @brief Initializes a new instance of the ByteConverter class. */
        ByteConverter() = default;
    };

} // namespace System::ComponentModel
