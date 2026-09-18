// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Double.hpp"
#include "System/ComponentModel/BaseNumberConverter.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a type converter to convert double-precision, floating point number objects to and from various other representations.
     *
     * C++ counterpart of .NET System.ComponentModel.DoubleConverter. Parses with
     * `System::Double::Parse(text, NumberStyles::Float, culture)` and formats with
     * `System::Double::ToString(value, "R", culture)`, so the culture's separators and signs are
     * honoured in both directions.
     */
    class DoubleConverter : public detail::NumberConverterBase<double, System::Double, false> {
    public:
        /** @brief Initializes a new instance of the DoubleConverter class. */
        DoubleConverter() = default;
    };

} // namespace System::ComponentModel
