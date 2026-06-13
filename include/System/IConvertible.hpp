// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/TypeCode.hpp"

namespace System {

    /// Defines methods that convert the value of the implementing reference or value type to a common language runtime type.
    class IConvertible {
    public:
        /// Virtual destructor for safe polymorphic destruction.
        virtual ~IConvertible() = default;
        /// Returns the TypeCode for this instance.
        [[nodiscard]] virtual TypeCode GetTypeCode() const = 0;
        /// Converts this value to a Boolean.
        [[nodiscard]] virtual bool ToBoolean() const = 0;
        /// Converts this value to a Unicode character.
        [[nodiscard]] virtual char ToChar() const = 0;
        /// Converts this value to an 8-bit signed integer.
        [[nodiscard]] virtual int8_t ToSByte() const = 0;
        /// Converts this value to an 8-bit unsigned integer.
        [[nodiscard]] virtual uint8_t ToByte() const = 0;
        /// Converts this value to a 16-bit signed integer.
        [[nodiscard]] virtual int16_t ToInt16() const = 0;
        /// Converts this value to a 16-bit unsigned integer.
        [[nodiscard]] virtual uint16_t ToUInt16() const = 0;
        /// Converts this value to a 32-bit signed integer.
        [[nodiscard]] virtual int32_t ToInt32() const = 0;
        /// Converts this value to a 32-bit unsigned integer.
        [[nodiscard]] virtual uint32_t ToUInt32() const = 0;
        /// Converts this value to a 64-bit signed integer.
        [[nodiscard]] virtual int64_t ToInt64() const = 0;
        /// Converts this value to a 64-bit unsigned integer.
        [[nodiscard]] virtual uint64_t ToUInt64() const = 0;
        /// Converts this value to a single-precision floating-point number.
        [[nodiscard]] virtual float ToSingle() const = 0;
        /// Converts this value to a double-precision floating-point number.
        [[nodiscard]] virtual double ToDouble() const = 0;
        /// Converts this value to a string.
        [[nodiscard]] virtual std::string ToString() const = 0;
    };

} // namespace System
