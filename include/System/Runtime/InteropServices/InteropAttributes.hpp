// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>
#include "System/Attribute.hpp"

namespace System::Runtime::InteropServices {

    enum class LayoutKind : int {
        Sequential = 0,
        Explicit   = 2,
        Auto       = 3
    };

    enum class CharSet : int {
        None    = 1,
        Ansi    = 2,
        Unicode = 3,
        Auto    = 4
    };

    enum class UnmanagedType : int {
        Bool       = 2,
        I1         = 3,
        U1         = 4,
        I2         = 5,
        U2         = 6,
        I4         = 7,
        U4         = 8,
        I8         = 9,
        U8         = 10,
        R4         = 11,
        R8         = 12,
        LPStr      = 20,
        LPWStr     = 21,
        LPTStr     = 22,
        ByValTStr  = 23,
        IUnknown   = 25,
        BStr       = 19,
        Struct     = 27,
        Interface  = 28,
        SafeArray  = 29,
        ByValArray = 30,
        SysInt     = 31,
        SysUInt    = 32,
        VBByRefStr = 34,
        AnsiBStr   = 35,
        TBStr      = 36,
        VariantBool= 37,
        FunctionPtr= 38,
        AsAny      = 40,
        LPArray    = 42,
        LPStruct   = 48,
        CustomMarshaler = 44,
        Error      = 45,
        IInspectable = 46,
        HString    = 47,
        LPUTF8Str  = 48
    };

    enum class CallingConvention : int {
        Winapi    = 1,
        Cdecl     = 2,
        StdCall   = 3,
        ThisCall  = 4,
        FastCall  = 5
    };

    class StructLayoutAttribute : public System::Attribute {
    public:
        LayoutKind Value;
        int    Pack    = 8;
        int    Size    = 0;
        ::System::Runtime::InteropServices::CharSet CharSet = ::System::Runtime::InteropServices::CharSet::Ansi;

        explicit StructLayoutAttribute(LayoutKind layout) : Value(layout) {}
        explicit StructLayoutAttribute(int16_t layout)    : Value(static_cast<LayoutKind>(layout)) {}
    };

    class FieldOffsetAttribute : public System::Attribute {
    public:
        int Value;
        explicit FieldOffsetAttribute(int offset) : Value(offset) {}
    };

    class MarshalAsAttribute : public System::Attribute {
    public:
        UnmanagedType Value;
        int ArraySubType = 0;
        std::string MarshalType;
        std::string MarshalTypeRef;
        std::string MarshalCookie;
        int SizeConst     = 0;
        int SizeParamIndex = 0;

        explicit MarshalAsAttribute(UnmanagedType t) : Value(t) {}
        explicit MarshalAsAttribute(int16_t t) : Value(static_cast<UnmanagedType>(t)) {}
    };

    class DllImportAttribute : public System::Attribute {
    public:
        std::string Value;
        std::string EntryPoint;
        ::System::Runtime::InteropServices::CharSet            CharSet           = ::System::Runtime::InteropServices::CharSet::None;
        ::System::Runtime::InteropServices::CallingConvention  CallingConvention = ::System::Runtime::InteropServices::CallingConvention::Winapi;
        bool               SetLastError      = false;
        bool               ExactSpelling     = false;
        bool               PreserveSig       = true;
        bool               BestFitMapping    = true;
        bool               ThrowOnUnmappableChar = false;

        explicit DllImportAttribute(const std::string& dllName) : Value(dllName) {}
    };

    class ComVisibleAttribute : public System::Attribute {
    public:
        bool Value;
        explicit ComVisibleAttribute(bool visible) : Value(visible) {}
    };

    class GuidAttribute : public System::Attribute {
    public:
        std::string Value;
        explicit GuidAttribute(const std::string& guid) : Value(guid) {}
    };

    class InterfaceTypeAttribute : public System::Attribute {
    public:
        int Value; // ComInterfaceType enum value
        explicit InterfaceTypeAttribute(int interfaceType) : Value(interfaceType) {}
    };

    class ClassInterfaceAttribute : public System::Attribute {
    public:
        int Value; // ClassInterfaceType enum value
        explicit ClassInterfaceAttribute(int classInterfaceType) : Value(classInterfaceType) {}
    };

    class InAttribute  : public System::Attribute {};
    class OutAttribute : public System::Attribute {};

    class OptionalAttribute : public System::Attribute {};

} // namespace System::Runtime::InteropServices
