# Audit: `modules/runtime/include/System/Runtime/InteropServices/RuntimeInformation.hpp`

## Metadata

- AUDITED: 36-line public declaration, fully read.
- Validation: shared Architecture/OSPlatform/RuntimeInformation filter passed
  11/11 on 2026-07-27.
- Reference basis: local current System.Runtime reference API and
  `RuntimeInformation.cs`.

## SR-AUD-153 — medium — RuntimeInformation omits public FrameworkDescription and RuntimeIdentifier properties

Current .NET exposes queryable FrameworkDescription and RuntimeIdentifier in
addition to OSDescription, process/OS architecture, and IsOSPlatform. C++
omits both and documents the absence as a native-build rationale, but callers
cannot even receive a documented unsupported/empty value. This is a public API
gap, not merely a different description string.

## Other missing assertions and diagnostics

- Tests only call the remaining four APIs, so neither absent property has a
  compile-time baseline or documented adaptation behavior.
- The class's deleted constructor is appropriate for a static managed class.

## Final assessment

Implemented APIs are well delimited, but two public runtime identity queries
are unavailable. No source or test was modified during this audit.
