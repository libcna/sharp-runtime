// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/ComponentModel/detail/ObjectText.hpp"

#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Byte.hpp"
#include "System/Double.hpp"
#include "System/Globalization/CultureInfo.hpp"
#include "System/Int16.hpp"
#include "System/Int32.hpp"
#include "System/Int64.hpp"
#include "System/SByte.hpp"
#include "System/Single.hpp"
#include "System/UInt16.hpp"
#include "System/UInt32.hpp"
#include "System/UInt64.hpp"

namespace System::ComponentModel::detail {

bool ObjectText::IsStringType(const System::Type& type) {
    return type == System::Type::From<std::string>() ||
           type == System::Type::From<std::string_view>() ||
           type == System::Type::From<const char*>() ||
           type == System::Type::From<char*>();
}

std::optional<std::string> ObjectText::AsString(const std::any& value) {
    if (const auto* s = std::any_cast<std::string>(&value)) return *s;
    if (const auto* sv = std::any_cast<std::string_view>(&value)) return std::string(*sv);
    if (const auto* cs = std::any_cast<const char*>(&value)) {
        return *cs != nullptr ? std::string(*cs) : std::string{};
    }
    if (const auto* ms = std::any_cast<char*>(&value)) {
        return *ms != nullptr ? std::string(*ms) : std::string{};
    }
    return std::nullopt;
}

std::string ObjectText::TypeName(const std::any& value) {
    if (!value.has_value()) return "(null)";
    return System::Type::FromTypeInfo(value.type()).getFullNameProperty();
}

std::string ObjectText::ToString(const std::any& value,
                                 const System::Globalization::CultureInfo* culture) {
    if (!value.has_value()) return {};
    const System::IFormatProvider* provider =
        culture != nullptr ? culture : &System::Globalization::CultureInfo::getCurrentCultureProperty();
    if (auto text = AsString(value)) return *text;
    if (const auto* v = std::any_cast<bool>(&value)) return *v ? "True" : "False";
    if (const auto* v = std::any_cast<float>(&value)) return System::Single::ToString(*v, provider);
    if (const auto* v = std::any_cast<double>(&value)) return System::Double::ToString(*v, provider);
    if (const auto* v = std::any_cast<SharpRuntime::intcs>(&value)) return System::Int32::ToString(*v, provider);
    if (const auto* v = std::any_cast<SharpRuntime::bytecs>(&value)) return System::Byte::ToString(*v, provider);
    if (const auto* v = std::any_cast<SharpRuntime::sbytecs>(&value)) return System::SByte::ToString(*v, provider);
    if (const auto* v = std::any_cast<SharpRuntime::shortcs>(&value)) return System::Int16::ToString(*v, provider);
    if (const auto* v = std::any_cast<SharpRuntime::ushortcs>(&value)) return System::UInt16::ToString(*v, provider);
    if (const auto* v = std::any_cast<SharpRuntime::uintcs>(&value)) return System::UInt32::ToString(*v, provider);
    if (const auto* v = std::any_cast<SharpRuntime::longcs>(&value)) return System::Int64::ToString(*v, provider);
    if (const auto* v = std::any_cast<SharpRuntime::ulongcs>(&value)) return System::UInt64::ToString(*v, provider);
    if (const auto* v = std::any_cast<char>(&value)) return std::string(1, *v);
    return TypeName(value);
}

} // namespace System::ComponentModel::detail
