// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System {

    enum class AttributeTargets {
        Assembly         = 0x0001,
        Module           = 0x0002,
        Class            = 0x0004,
        Struct           = 0x0008,
        Enum             = 0x0010,
        Constructor      = 0x0020,
        Method           = 0x0040,
        Property         = 0x0080,
        Field            = 0x0100,
        Event            = 0x0200,
        Interface        = 0x0400,
        Parameter        = 0x0800,
        Delegate         = 0x1000,
        ReturnValue      = 0x2000,
        GenericParameter = 0x4000,
        All = Assembly | Module | Class | Struct | Enum | Constructor |
              Method | Property | Field | Event | Interface | Parameter |
              Delegate | ReturnValue | GenericParameter
    };

    inline AttributeTargets operator|(AttributeTargets a, AttributeTargets b) {
        return static_cast<AttributeTargets>(static_cast<int>(a) | static_cast<int>(b));
    }
    inline AttributeTargets operator&(AttributeTargets a, AttributeTargets b) {
        return static_cast<AttributeTargets>(static_cast<int>(a) & static_cast<int>(b));
    }

} // namespace System
