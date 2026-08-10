# Audit: `modules/uri/include/System/UriFormatException.hpp`

## Metadata

- AUDITED: 32-line inline exception declaration, fully read.
- Validation: `UriFormatExceptionTest.*` passed 6/6 within the selected 13-test
  URI converter/exception filter on 2026-07-27.
- Reference basis: local current `UriFormatException.cs`.

## Assessment

The default, message, and message-plus-inner C++ constructors correctly retain
the FormatException behavior expected by the representable native adaptation.
No standalone constructor defect was reproduced.

## Other missing assertions and diagnostics

- Tests omit inherited HResult, stored-inner identity/rethrow, empty/UTF-8
  message boundaries, and copy/move behavior.
- Current .NET also exposes obsolete formatter-serialization members and
  ISerializable; the native project has no compatible serialization surface,
  so this is an explicit adaptation/API-baseline question rather than a
  constructor failure.
- No parser route is checked to ensure malformed URI text consistently creates
  this type rather than a native exception.

## Final assessment

The compact exception wrapper is coherent for ordinary construction. No source
or test was modified during this audit.
