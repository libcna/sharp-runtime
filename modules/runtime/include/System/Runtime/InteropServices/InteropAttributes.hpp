// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <cstdint>
#include <string>
#include "SharpRuntime/SharpRuntimeHelper.hpp"
#include "System/Attribute.hpp"

namespace System::Runtime::InteropServices {

    /**
     * @file InteropAttributes.hpp
     *
     * @warning **Every type in this header is an inert metadata object. None of the layout,
     * marshalling, calling-convention or COM effects their names and doc-comments describe
     * happens in this port.**
     *
     * There is deliberately no declaration-attachment syntax, no ABI or field-layout
     * transformation, no DLL binding or symbol resolution, and no COM marshaller behind any of
     * them. Constructing a `StructLayoutAttribute`, `FieldOffsetAttribute`, `MarshalAsAttribute`,
     * `DllImportAttribute`, `GuidAttribute`, `ComVisibleAttribute`, `InAttribute`,
     * `OutAttribute` or `OptionalAttribute` stores values and does nothing else — it cannot
     * change how a C++ declaration is laid out, how a native call is made, or how anything is
     * marshalled. In C# these attributes are read by the compiler and the CLR; C++ has no
     * equivalent consumer, and this port does not provide one.
     *
     * That is not an omission to be fixed here. `CLAUDE.md`'s "Known permanent deviations"
     * classifies **P/Invoke and interop as out of scope**, so these types exist to let ported
     * declarations keep compiling and to preserve the managed metadata values, not to reproduce
     * managed interop behaviour. Use the platform's own mechanisms — `extern "C"`, `alignas`,
     * `#pragma pack`, `dlopen`/`LoadLibrary` — when a real native effect is needed.
     *
     * Stated explicitly because the audit found this header to be the one interop-adjacent file
     * that described effects it cannot produce **without saying so**, unlike the compiler-service
     * marker headers alongside it (ticket #1978, the disclosure half of SR-AUD-168). The
     * *value and type* divergences this header used to declare — `UnmanagedType::LPStruct`, the
     * missing `Currency`/`IDispatch`, `StructLayoutAttribute::Pack`, both `CharSet` defaults and
     * `DllImportAttribute::PreserveSig`/`BestFitMapping`, the typed `MarshalAsAttribute` fields,
     * and the COM-interface attribute values — were **#1980 groups G-2/G-5** and are now fixed;
     * see docs/Migration-InteropMetadataValues.md and docs/Migration-MarshalAsFieldTypes.md.
     *
     * Because these types exist to preserve managed metadata rather than to produce an effect,
     * getting the metadata values and their public types right is the whole of their contract.
     */

    /** Specifies the memory layout of a managed class or struct. */
    enum class LayoutKind : SharpRuntime::intcs {
        Sequential = 0, ///< Members laid out sequentially, as they appear in the source.
        Explicit   = 2, ///< Each member has an explicitly specified offset.
        Auto       = 3  ///< The runtime chooses the layout automatically.
    };

    /** Specifies the character set used when marshalling strings. */
    enum class CharSet : SharpRuntime::intcs {
        None    = 1, ///< Not specified.
        Ansi    = 2, ///< ANSI (single-byte) strings.
        Unicode = 3, ///< Unicode (wide) strings.
        Auto    = 4  ///< Automatically select based on platform.
    };

    /** Specifies the unmanaged type to marshal a managed type to/from. */
    enum class UnmanagedType : SharpRuntime::intcs {
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
        // #1980 G-2 / SR-AUD-165: Currency and IDispatch were absent. .NET declares
        // `Currency = 0xf` and `IDispatch = 0x1a` (UnmanagedType.cs:22,30).
        Currency   = 15,
        IDispatch  = 26,
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
        // #1980 G-2 / SR-AUD-165. This was 48, which is not merely the wrong number: 48 is
        // LPUTF8Str's value, so the two enumerators COLLIDED -- `LPStruct == LPUTF8Str` was true,
        // and a switch over UnmanagedType could not carry both arms. .NET declares
        // `LPStruct = 0x2b` (UnmanagedType.cs:55). The plan recorded this as a wrong value; the
        // collision is what measurement added.
        LPStruct   = 43,
        CustomMarshaler = 44,
        Error      = 45,
        IInspectable = 46,
        HString    = 47,
        LPUTF8Str  = 48
    };

    /**
     * @brief COM automation variant types.
     *
     * #1980 group G-5 / SR-AUD-167: added because `MarshalAsAttribute::SafeArraySubType` is a
     * `VarEnum` in .NET (`MarshalAsAttribute.cs:21`) and neither the field nor its type existed
     * here. Transcribed from `VarEnum.cs`, including the three flag values above 0x1000.
     */
    enum class VarEnum : SharpRuntime::intcs {
        VT_EMPTY = 0,   VT_NULL = 1,    VT_I2 = 2,      VT_I4 = 3,
        VT_R4 = 4,      VT_R8 = 5,      VT_CY = 6,      VT_DATE = 7,
        VT_BSTR = 8,    VT_DISPATCH = 9, VT_ERROR = 10, VT_BOOL = 11,
        VT_VARIANT = 12, VT_UNKNOWN = 13, VT_DECIMAL = 14,
        // 15 is deliberately absent: .NET's enum has no member with that value.
        VT_I1 = 16,     VT_UI1 = 17,    VT_UI2 = 18,    VT_UI4 = 19,
        VT_I8 = 20,     VT_UI8 = 21,    VT_INT = 22,    VT_UINT = 23,
        VT_VOID = 24,   VT_HRESULT = 25, VT_PTR = 26,   VT_SAFEARRAY = 27,
        VT_CARRAY = 28, VT_USERDEFINED = 29, VT_LPSTR = 30, VT_LPWSTR = 31,
        VT_RECORD = 36, VT_FILETIME = 64, VT_BLOB = 65, VT_STREAM = 66,
        VT_STORAGE = 67, VT_STREAMED_OBJECT = 68, VT_STORED_OBJECT = 69,
        VT_BLOB_OBJECT = 70, VT_CF = 71, VT_CLSID = 72,
        VT_VECTOR = 0x1000, VT_ARRAY = 0x2000, VT_BYREF = 0x4000
    };

    /**
     * @brief How a COM interface is exposed to COM clients.
     *
     * #1980 group G-5 / SR-AUD-167. Transcribed from `ComInterfaceType.cs`.
     */
    enum class ComInterfaceType : SharpRuntime::intcs {
        InterfaceIsDual         = 0,
        InterfaceIsIUnknown     = 1,
        InterfaceIsIDispatch    = 2,
        InterfaceIsIInspectable = 3
    };

    /**
     * @brief The kind of class interface generated for a class.
     *
     * #1980 group G-5 / SR-AUD-167. Transcribed from `ClassInterfaceType.cs`.
     */
    enum class ClassInterfaceType : SharpRuntime::intcs {
        None         = 0,
        AutoDispatch = 1,
        AutoDual     = 2
    };

    /** Specifies the calling convention of an unmanaged entry point. */
    enum class CallingConvention : SharpRuntime::intcs {
        Winapi    = 1, ///< Platform default (stdcall on Windows).
        Cdecl     = 2,
        StdCall   = 3,
        ThisCall  = 4,
        FastCall  = 5
    };

    /** Specifies the memory layout model of a managed struct or class for interop. */
    class StructLayoutAttribute : public System::Attribute {
    public:
        LayoutKind Value;  ///< The layout kind.
        /**
         * Packing alignment in bytes.
         *
         * #1980 G-2 / SR-AUD-166: this defaulted to **8**. .NET declares `public int Pack;` with
         * no initializer (`StructLayoutAttribute.cs:21`), so a default-constructed attribute
         * reports **0** -- which in the metadata means "use the runtime's default packing", a
         * different statement from "pack to 8".
         */
        SharpRuntime::intcs Pack = 0;
        SharpRuntime::intcs Size = 0; ///< Minimum size in bytes (0 = no minimum).
        /**
         * Character set used for embedded strings.
         *
         * #1980 G-2: this defaulted to `CharSet::Ansi`. .NET declares `public CharSet CharSet;`
         * with no initializer (`StructLayoutAttribute.cs:23`), so the default is **0** -- and
         * `CharSet` has no enumerator with that value (`None` is 1). **That is deliberate, not a
         * slip**: this header exists to preserve the managed metadata values exactly, and .NET's
         * metadata really does carry an unset CharSet here. The plan's G-2 list did not name this
         * field; it was found by measuring the reference alongside the four it did name.
         */
        ::System::Runtime::InteropServices::CharSet CharSet =
            static_cast<::System::Runtime::InteropServices::CharSet>(0);

        /** @param layout The desired memory layout kind. */
        explicit StructLayoutAttribute(LayoutKind layout) : Value(layout) {}

        /** Integer overload — @param layout is cast to LayoutKind. */
        explicit StructLayoutAttribute(SharpRuntime::shortcs layout) : Value(static_cast<LayoutKind>(layout)) {}
    };

    /** Specifies the field offset within a struct that uses explicit layout. */
    class FieldOffsetAttribute : public System::Attribute {
    public:
        SharpRuntime::intcs Value; ///< Byte offset of the field from the start of the struct.

        /** @param offset Byte offset from the start of the struct. */
        explicit FieldOffsetAttribute(SharpRuntime::intcs offset) : Value(offset) {}
    };

    /** Indicates how a managed member should be marshalled to/from unmanaged code. */
    class MarshalAsAttribute final : public System::Attribute {
        UnmanagedType value_;
    public:
        /**
         * Element type for array marshalling.
         *
         * #1980 group G-5 / SR-AUD-167: this was `intcs`. .NET declares
         * `public UnmanagedType ArraySubType;` (`MarshalAsAttribute.cs:29`) -- an
         * `UnmanagedType`, not a loose integer. The weaker type let any number be stored where
         * only a marshalling kind is meaningful, which is the whole reason the enum exists.
         *
         * The default is `0`, which is **not a declared enumerator** (`UnmanagedType` starts at
         * `Bool = 2`) -- deliberately, because .NET's field has no initializer either, the same
         * reasoning group G-2 recorded for the two `CharSet` defaults.
         */
        UnmanagedType ArraySubType = static_cast<UnmanagedType>(0);
        /**
         * The COM variant type of a SafeArray's elements.
         *
         * #1980 G-5 / SR-AUD-167: absent before. .NET: `public VarEnum SafeArraySubType;`
         * (`MarshalAsAttribute.cs:21`).
         */
        VarEnum SafeArraySubType = static_cast<VarEnum>(0);
        /**
         * Parameter index of the IID for an `IUnknown`/`IDispatch` marshal.
         *
         * #1980 G-5 / SR-AUD-167: absent before. .NET: `public int IidParameterIndex;`
         * (`MarshalAsAttribute.cs:25`).
         */
        SharpRuntime::intcs IidParameterIndex = 0;
        std::string MarshalType;      ///< Fully qualified name of a custom marshaller.
        /**
         * Type reference for a custom marshaller.
         *
         * @note **A permanent deviation, stated rather than left implicit.** .NET declares this
         * as `public Type? MarshalTypeRef;` (`MarshalAsAttribute.cs:35`), and `System::Type` is
         * reflection -- out of scope for this port by explicit decision. A `std::string` holding
         * the type's name is the closest available shape. The same applies to
         * `SafeArrayUserDefinedSubType`, which .NET also types as `Type?` and which is therefore
         * **absent here** rather than invented as another string: adding a member that cannot
         * carry what .NET's carries would be worse than not having it.
         */
        std::string MarshalTypeRef;
        std::string MarshalCookie;    ///< Extra string passed to the custom marshaller.
        SharpRuntime::intcs SizeConst     = 0; ///< Fixed array/string size for ByValArray/ByValTStr.
        /**
         * Parameter index supplying the array size.
         *
         * #1980 G-5 / SR-AUD-167: this was `intcs`. .NET declares
         * `public short SizeParamIndex;` (`MarshalAsAttribute.cs:30`) -- a **short**, because the
         * value is a parameter position and the metadata encoding is 16-bit.
         */
        SharpRuntime::shortcs SizeParamIndex = 0;

        /** @param t The unmanaged marshalling type. */
        explicit MarshalAsAttribute(UnmanagedType t) : value_(t) {}

        /** Integer overload — @param t is cast to UnmanagedType. */
        explicit MarshalAsAttribute(SharpRuntime::shortcs t) : value_(static_cast<UnmanagedType>(t)) {}

        /**
         * @return The unmanaged type to marshal as.
         *
         * #1980 G-5 / SR-AUD-167: `Value` was a public **mutable** data member. .NET's is
         * `public UnmanagedType Value { get; }` (`MarshalAsAttribute.cs:18`) -- get-only, set
         * once by the constructor. Landed under SA-8, whose first bullet is exactly this shape.
         */
        [[nodiscard]] UnmanagedType getValueProperty() const noexcept { return value_; }
    };

    /** Specifies the DLL entry point and calling options for a P/Invoke method. */
    class DllImportAttribute : public System::Attribute {
    public:
        std::string Value;         ///< The name of the DLL to import from.
        std::string EntryPoint;    ///< Name of the exported function; empty means use the method name.
        /**
         * String marshalling character set.
         *
         * #1980 G-2: this defaulted to `CharSet::None` (1). .NET declares
         * `public CharSet CharSet;` with no initializer, so the default is **0**, which is not a
         * declared enumerator. Same reasoning as `StructLayoutAttribute::CharSet` above.
         */
        ::System::Runtime::InteropServices::CharSet            CharSet           =
            static_cast<::System::Runtime::InteropServices::CharSet>(0);
        ::System::Runtime::InteropServices::CallingConvention  CallingConvention = ::System::Runtime::InteropServices::CallingConvention::Winapi; ///< Calling convention.
        bool               SetLastError      = false; ///< Capture GetLastError after the call.
        bool               ExactSpelling     = false; ///< Disable automatic A/W suffix probing.
        /**
         * Preserve the signature (no HRESULT transformation).
         *
         * #1980 G-2 / SR-AUD-166: this defaulted to **true**. .NET declares `public bool
         * PreserveSig;` with no initializer (`DllImportAttribute.cs:22`), so the field reads
         * **false** on a default-constructed attribute -- the same as every other bool on the
         * type, none of which this port had got wrong.
         */
        bool               PreserveSig       = false;
        /**
         * Enable best-fit character mapping.
         *
         * #1980 G-2 / SR-AUD-166: this defaulted to **true**; .NET's is a plain
         * `public bool BestFitMapping;` (`DllImportAttribute.cs:21`), i.e. **false**.
         */
        bool               BestFitMapping    = false;
        bool               ThrowOnUnmappableChar = false; ///< Throw on unmappable Unicode characters.

        /** @param dllName The name of the native DLL. */
        explicit DllImportAttribute(const std::string& dllName) : Value(dllName) {}
    };

    /** Controls the COM visibility of an individual managed type or member. */
    class ComVisibleAttribute : public System::Attribute {
    public:
        bool Value; ///< True if visible to COM.

        /** @param visible True to expose the type/member to COM. */
        explicit ComVisibleAttribute(bool visible) : Value(visible) {}
    };

    /** Specifies the GUID of the attributed type or interface. */
    class GuidAttribute : public System::Attribute {
    public:
        std::string Value; ///< The GUID string.

        /** @param guid The GUID in registry format (e.g. "00000000-0000-0000-0000-000000000000"). */
        explicit GuidAttribute(const std::string& guid) : Value(guid) {}
    };

    /** Indicates the COM interface type exposed by a managed interface. */
    class InterfaceTypeAttribute final : public System::Attribute {
        ComInterfaceType value_;

    public:
        /** @param interfaceType The strongly typed COM interface kind. */
        explicit InterfaceTypeAttribute(ComInterfaceType interfaceType) noexcept
            : value_(interfaceType) {}

        /** @param interfaceType Raw 16-bit metadata value accepted by .NET's compatibility overload. */
        explicit InterfaceTypeAttribute(SharpRuntime::shortcs interfaceType) noexcept
            : value_(static_cast<ComInterfaceType>(interfaceType)) {}

        /** @return The immutable COM interface kind selected at construction. */
        [[nodiscard]] ComInterfaceType getValueProperty() const noexcept { return value_; }
    };

    /** Specifies the type of COM interface generated for a class. */
    class ClassInterfaceAttribute final : public System::Attribute {
        ClassInterfaceType value_;

    public:
        /** @param classInterfaceType The strongly typed generated class-interface kind. */
        explicit ClassInterfaceAttribute(ClassInterfaceType classInterfaceType) noexcept
            : value_(classInterfaceType) {}

        /** @param classInterfaceType Raw 16-bit metadata value accepted by .NET's compatibility overload. */
        explicit ClassInterfaceAttribute(SharpRuntime::shortcs classInterfaceType) noexcept
            : value_(static_cast<ClassInterfaceType>(classInterfaceType)) {}

        /** @return The immutable generated class-interface kind selected at construction. */
        [[nodiscard]] ClassInterfaceType getValueProperty() const noexcept { return value_; }
    };

    /** Marks a parameter as input-only in a COM interop signature. */
    class InAttribute  : public System::Attribute {};

    /** Marks a parameter as output-only in a COM interop signature. */
    class OutAttribute : public System::Attribute {};

    /** Marks a parameter as optional in a COM interop signature. */
    class OptionalAttribute : public System::Attribute {};

} // namespace System::Runtime::InteropServices
