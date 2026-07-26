# Audit: `modules/runtime/include/System/Runtime/InteropServices/InteropAttributes.hpp`

## Metadata

- AUDITED: 180-line inline interop enum/attribute declaration, fully read.
- Validation: all eleven interop fixture suites passed 23/23 on 2026-07-27.
- Reference/probe: local current-.NET InteropServices declarations and unit
  tests; C++ probe prints `lpstruct=48`, `pack=8`, `preservesig=1`, and
  `bestfit=1`, while the matching managed probe prints `43`, `0`, `False`, and
  `False` respectively.

## SR-AUD-165 — medium — UnmanagedType misencodes LPStruct and omits managed enum members

`LPStruct` is assigned `48`, colliding with `LPUTF8Str`; current .NET assigns
it `43` (`0x2b`) and reserves `48` for UTF-8 strings.  The C++ probe confirms
the collision, while the managed probe reports `LPStruct=43` and
`LPUTF8Str=48`.  C++ also omits current .NET's `Currency=15` and
`IDispatch=26` values.

Code selecting LPStruct therefore advertises an incompatible marshalling code,
and code referencing either absent public enum member cannot compile.  The
direct tests assert only Bool and LPStr, leaving both failures unguarded.

## SR-AUD-166 — medium — StructLayout and DllImport defaults diverge, including test-locked Pack=8

Current .NET constructed attributes default `StructLayout.Pack` to `0` (the
platform/default packing sentinel) and DllImport `PreserveSig` and
`BestFitMapping` to `false`.  C++ initializes the same public fields to `8`,
`true`, and `true`.  The C++/managed probe demonstrates all three values, and
`StructLayoutAttributeTests.DefaultPack_IsEight` explicitly preserves the
wrong C++ default.

This changes the declarative ABI/marshalling metadata callers observe before
they set a field themselves.

## SR-AUD-167 — medium — MarshalAs and COM-interface attribute surfaces lose typed fields and public enum contracts

MarshalAs omits `SafeArraySubType`, `SafeArrayUserDefinedSubType`, and
`IidParameterIndex`; it changes `ArraySubType` from `UnmanagedType` to an
untyped integer, `SizeParamIndex` from `short` to `intcs`, and
`MarshalTypeRef` from nullable `System::Type` to a string.  The header also
omits the public `ComInterfaceType` and `ClassInterfaceType` enums, replacing
both attribute constructors and Value properties with unrestricted `intcs`.

Those changes remove source-compatible type safety and several observable
metadata fields; the aggregate tests never instantiate either COM-interface
attribute or inspect any omitted MarshalAs member.

## SR-AUD-168 — medium — interop attributes are detached data objects with no native declaration, marshalling, or P/Invoke consumer

All types in this header are ordinary C++ objects.  Searches find no production
consumer beyond the shared construction fixture, no declaration-attachment
syntax, no ABI layout transformation, no DLL binding generation, and no COM
marshaller.  Thus setting StructLayout, FieldOffset, MarshalAs, DllImport,
Guid, In/Out/Optional, or COM visibility/interface fields cannot affect an
attributed C++ declaration or native call.

Current .NET attaches these metadata types to specific declarations and uses
them in interop tooling/runtime behavior.  Unlike the compiler-service marker
headers, this file does not document an explicit native alternative or warn
that its stated layout/P/Invoke effects are unavailable.

## Other missing assertions and diagnostics

- Tests sample only 9 enum values and a handful of nominal fields; they omit
  LPStruct, all omitted UnmanagedType members, every default DllImport field
  other than SetLastError, and StructLayout CharSet/raw-short construction.
- They omit MarshalAs's raw-short constructor and all fields beyond Value and
  SizeConst, InterfaceType/ClassInterface construction, and every nullable
  string/type state.
- Marker tests end in `SUCCEED()` and have no declaration attachment, ABI
  layout, DLL lookup, error-code, COM visibility, or native marshalling probe.

## Final assessment

Some basic enum values and stored fields are present, but the header contains
incorrect defaults/enum data, a truncated typed metadata surface, and no
operational interop consumer.  No source or test was modified.
