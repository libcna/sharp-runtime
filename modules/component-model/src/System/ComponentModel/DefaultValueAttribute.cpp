// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#include "System/ComponentModel/DefaultValueAttribute.hpp"

#include <exception>
#include "System/ComponentModel/TypeConverter.hpp"
#include "System/ComponentModel/TypeDescriptor.hpp"

namespace System::ComponentModel {

DefaultValueAttribute::DefaultValueAttribute(const System::Type& type, std::optional<std::string> value) {
    // DefaultValueAttribute.cs: `try { Value = TypeDescriptor.GetConverter(type)
    // .ConvertFromInvariantString(value); } catch { }` -- a failed conversion leaves Value null.
    if (!value.has_value()) return;
    try {
        value_ = TypeDescriptor::GetConverter(type)->ConvertFromInvariantString(*value);
    } catch (...) {
        value_.reset();
    }
}

} // namespace System::ComponentModel
