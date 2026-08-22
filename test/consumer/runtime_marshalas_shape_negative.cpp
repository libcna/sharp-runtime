// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
//
// Negative compile fixture for ticket #1980 group G-5 (SR-AUD-167).
//
// G-5 gave MarshalAsAttribute .NET's field TYPES, added the two absent COM enums, and now applies
// those enums to their two owning attribute classes. Like G-2, this is metadata fidelity rather
// than style: a field typed as a loose integer where .NET types it as an enum lets any number be
// stored where only a marshalling kind is meaningful, which is the whole reason the enum exists.
//
//   * `Value` was a public MUTABLE data member; .NET's is `public UnmanagedType Value { get; }`
//     (MarshalAsAttribute.cs:18). Read it with getValueProperty().
//   * `ArraySubType` was `intcs`; .NET's is `UnmanagedType` (MarshalAsAttribute.cs:29).
//   * `SizeParamIndex` was `intcs`; .NET's is `short` (MarshalAsAttribute.cs:30).
//   * The class is now final, matching .NET's `sealed`.
//   * InterfaceTypeAttribute and ClassInterfaceAttribute likewise become final and expose a
//     typed get-only Value through getValueProperty(); both retain .NET's enum and raw-short
//     construction routes.
//
// Migration: `attr.Value` becomes `attr.getValueProperty()`; assign `UnmanagedType` values to
// ArraySubType rather than integers.
//
// Records: docs/Migration-MarshalAsFieldTypes.md, docs/NegativeConsumerFixtureValidation.md.
//
// NEGATIVE-FIXTURE: component=Runtime
#include <type_traits>

#include "System/Runtime/InteropServices/InteropAttributes.hpp"

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

using System::Runtime::InteropServices::ClassInterfaceType;
using System::Runtime::InteropServices::ClassInterfaceAttribute;
using System::Runtime::InteropServices::ComInterfaceType;
using System::Runtime::InteropServices::InterfaceTypeAttribute;
using System::Runtime::InteropServices::MarshalAsAttribute;
using System::Runtime::InteropServices::UnmanagedType;
using System::Runtime::InteropServices::VarEnum;

int main() {
    MarshalAsAttribute attr(UnmanagedType::LPArray);

#if SHARP_RUNTIME_NEGATIVE_SITE == 1
    // NEGATIVE(marshalas-value-read): has no member named 'Value'
    //     | no member named
    //     | is private
    UnmanagedType read = attr.Value;
    (void)read;
#else
    UnmanagedType read = attr.getValueProperty();
    (void)read;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(marshalas-value-write): has no member named 'Value'
    //     | no member named
    //     | is private
    // THE SPELLING THIS TICKET EXISTS FOR: Value was assignable, so a caller could retarget an
    // attribute after construction. .NET's is get-only.
    attr.Value = UnmanagedType::I4;
#else
    attr = MarshalAsAttribute(UnmanagedType::I4);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 3
    // NEGATIVE(marshalas-arraysubtype-int): cannot convert
    //     | invalid conversion
    //     | no known conversion
    // The weaker type is what let any number be stored here.
    attr.ArraySubType = 7;
#else
    attr.ArraySubType = UnmanagedType::I4;
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 4
    // NEGATIVE(marshalas-derive): cannot derive
    //     | is marked 'final'
    //     | final
    struct Derived : MarshalAsAttribute { Derived() : MarshalAsAttribute(UnmanagedType::I4) {} };
    (void)sizeof(Derived);
#else
    static_assert(std::is_final_v<MarshalAsAttribute>, "#1980 G-5: .NET's MarshalAsAttribute is sealed");
#endif

    InterfaceTypeAttribute interfaceAttribute(ComInterfaceType::InterfaceIsIDispatch);
#if SHARP_RUNTIME_NEGATIVE_SITE == 5
    // NEGATIVE(interfacetype-value-read): has no member named 'Value'
    //     | no member named
    //     | is private
    const ComInterfaceType interfaceValue = interfaceAttribute.Value;
#else
    const ComInterfaceType interfaceValue = interfaceAttribute.getValueProperty();
#endif

    ClassInterfaceAttribute classAttribute(ClassInterfaceType::AutoDual);
#if SHARP_RUNTIME_NEGATIVE_SITE == 6
    // NEGATIVE(classinterface-value-write): has no member named 'Value'
    //     | no member named
    //     | is private
    classAttribute.Value = ClassInterfaceType::None;
    const ClassInterfaceType classValue = ClassInterfaceType::None;
#else
    const ClassInterfaceType classValue = classAttribute.getValueProperty();
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 7
    // NEGATIVE(interfacetype-derive): cannot derive
    //     | is marked 'final'
    //     | final
    struct DerivedInterfaceTypeAttribute : InterfaceTypeAttribute {
        DerivedInterfaceTypeAttribute()
            : InterfaceTypeAttribute(ComInterfaceType::InterfaceIsIUnknown) {}
    };
    (void)sizeof(DerivedInterfaceTypeAttribute);
#else
    static_assert(std::is_final_v<InterfaceTypeAttribute>);
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 8
    // NEGATIVE(classinterface-derive): cannot derive
    //     | is marked 'final'
    //     | final
    struct DerivedClassInterfaceAttribute : ClassInterfaceAttribute {
        DerivedClassInterfaceAttribute()
            : ClassInterfaceAttribute(ClassInterfaceType::AutoDispatch) {}
    };
    (void)sizeof(DerivedClassInterfaceAttribute);
#else
    static_assert(std::is_final_v<ClassInterfaceAttribute>);
#endif

    const InterfaceTypeAttribute rawInterface(static_cast<SharpRuntime::shortcs>(3));
    const ClassInterfaceAttribute rawClass(static_cast<SharpRuntime::shortcs>(1));

    // UNCHANGED, and asserted so the fixture proves what did NOT break: the other fields are
    // still public and writable, and the two new enums are usable.
    attr.SizeConst = 4;
    attr.SizeParamIndex = 2;
    attr.IidParameterIndex = 1;
    attr.SafeArraySubType = VarEnum::VT_BSTR;
    attr.MarshalType = "Custom";
    attr.MarshalTypeRef = "Custom";
    attr.MarshalCookie = "cookie";
    const auto com = ComInterfaceType::InterfaceIsIDispatch;
    const auto cls = ClassInterfaceType::AutoDual;
    return (attr.getValueProperty() == UnmanagedType::I4 && attr.SizeConst == 4 &&
            static_cast<int>(com) == 2 && static_cast<int>(cls) == 2 &&
            interfaceValue == ComInterfaceType::InterfaceIsIDispatch &&
            classValue == ClassInterfaceType::AutoDual &&
            rawInterface.getValueProperty() == ComInterfaceType::InterfaceIsIInspectable &&
            rawClass.getValueProperty() == ClassInterfaceType::AutoDispatch) ? 0 : 1;
}
