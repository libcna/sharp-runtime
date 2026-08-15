// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)

#include <cstdint>
#include <vector>

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/BitConverter.hpp"
#include "System/Buffers/Binary/BinaryPrimitives.hpp"
#include "System/DBNull.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/Int64.hpp"
#include "System/Math.hpp"
#include "System/TimeSpan.hpp"
#include "System/Xml/XmlConvert.hpp"

static_assert(SHARP_RUNTIME_HAS_NATIVE_INT128 == 0);
static_assert(sizeof(void*) == 4);

int main()
{
    const std::vector<SharpRuntime::bytecs> bytes{0x2A, 0x00, 0x00, 0x00};
    const auto hex = System::BitConverter::ToString(bytes);
    const auto parsed = System::Int64::Parse("42");
    const auto reversed = System::Buffers::Binary::BinaryPrimitives::ReverseEndianness(
        static_cast<std::uint32_t>(0x01020304u));
    const auto xmlValue = System::Xml::XmlConvert::ToInt32("42");
    const auto typeCode = System::DBNull::Value().GetTypeCode();

    return hex == "2A-00-00-00" && parsed == 42 && reversed == 0x04030201u &&
                   xmlValue == 42 && typeCode == System::TypeCode::DBNull
               ? 0
               : 1;
}
