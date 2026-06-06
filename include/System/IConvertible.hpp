// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/TypeCode.hpp"

namespace System {

    class IConvertible {
    public:
        virtual ~IConvertible() = default;
        [[nodiscard]] virtual TypeCode GetTypeCode() const = 0;
        [[nodiscard]] virtual bool ToBoolean() const = 0;
        [[nodiscard]] virtual char ToChar() const = 0;
        [[nodiscard]] virtual int8_t ToSByte() const = 0;
        [[nodiscard]] virtual uint8_t ToByte() const = 0;
        [[nodiscard]] virtual int16_t ToInt16() const = 0;
        [[nodiscard]] virtual uint16_t ToUInt16() const = 0;
        [[nodiscard]] virtual int32_t ToInt32() const = 0;
        [[nodiscard]] virtual uint32_t ToUInt32() const = 0;
        [[nodiscard]] virtual int64_t ToInt64() const = 0;
        [[nodiscard]] virtual uint64_t ToUInt64() const = 0;
        [[nodiscard]] virtual float ToSingle() const = 0;
        [[nodiscard]] virtual double ToDouble() const = 0;
        [[nodiscard]] virtual std::string ToString() const = 0;
    };

} // namespace System
