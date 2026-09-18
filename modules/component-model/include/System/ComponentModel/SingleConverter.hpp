// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Single.hpp"
#include "System/ComponentModel/BaseNumberConverter.hpp"

namespace System::ComponentModel {

    /**
     * @brief Provides a type converter to convert single-precision, floating point number objects to and from various other representations.
     *
     * C++ counterpart of .NET System.ComponentModel.SingleConverter. Parses with
     * `System::Single::Parse(text, NumberStyles::Float, culture)` and formats with
     * `System::Single::ToString(value, "R", culture)`, so the culture's separators and signs are
     * honoured in both directions.
     */
    class SingleConverter : public detail::NumberConverterBase<float, System::Single, false> {
    public:
        /** @brief Initializes a new instance of the SingleConverter class. */
        SingleConverter() = default;
    };

} // namespace System::ComponentModel
