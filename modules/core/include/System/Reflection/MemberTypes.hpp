// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once

namespace System::Reflection {

/**
 * @brief Marks each type of member that is defined as a derived class of MemberInfo.
 *
 * C++ counterpart of .NET System.Reflection.MemberTypes. The values are .NET's bit flags; this
 * runtime describes only the members a caller declares explicitly (see `MemberInfo`), so the
 * enumeration exists to name what such a description is, not to drive discovery.
 */
enum class MemberTypes {
    Constructor = 0x01, ///< Specifies that the member is a constructor.
    Event       = 0x02, ///< Specifies that the member is an event.
    Field       = 0x04, ///< Specifies that the member is a field.
    Method      = 0x08, ///< Specifies that the member is a method.
    Property    = 0x10, ///< Specifies that the member is a property.
    TypeInfo    = 0x20, ///< Specifies that the member is a type.
    Custom      = 0x40, ///< Specifies that the member is a custom member type.
    NestedType  = 0x80, ///< Specifies that the member is a nested type.
    All         = 0xBF, ///< Specifies all member types.
};

} // namespace System::Reflection
