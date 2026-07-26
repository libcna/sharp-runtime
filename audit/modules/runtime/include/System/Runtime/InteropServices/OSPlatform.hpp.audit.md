# Audit: `modules/runtime/include/System/Runtime/InteropServices/OSPlatform.hpp`

## Metadata

- AUDITED: 59-line inline value implementation, fully read.
- Validation: shared Architecture/OSPlatform/RuntimeInformation filter passed
  11/11 on 2026-07-27.
- Reference/probe: local `OSPlatform.cs`; a C++ default-value probe fails to
  compile, while matching C# prints an empty default text and false Linux
  equality.

## SR-AUD-152 — medium — OSPlatform cannot represent the valid default managed struct value

Current .NET exposes a readonly struct, so `default(OSPlatform)` is valid,
stringifies to empty text, and is unequal to Linux. C++ makes its sole
constructor private and takes a string; `OSPlatform platform;` fails with no
matching default constructor. This removes a public value state and prevents
generic/default-initialization code from expressing it.

## Other missing assertions and diagnostics

- Tests cover named values, case-insensitive equality, and hashes, but omit
  default construction/default equality and non-ASCII platform names.
- Null is not representable by the C++ string parameter; the native adaptation
  needs an explicit optional/sentinel policy if API parity is expanded.

## Final assessment

Named platform behavior is coherent, but the public default-value contract is
missing. No source or test was modified.
