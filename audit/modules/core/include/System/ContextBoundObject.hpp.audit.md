# Audit: `modules/core/include/System/ContextBoundObject.hpp`

## Metadata

- AUDITED: 29-line marker-base declaration, fully read.
- Validation: `ContextBoundObjectTests.*` passed 6/6 in the combined 14-test
  `ContextBoundObjectTests.*:LocalDataStoreSlotTests.*:MarshalByRefObjectNewTests.*`
  Core.Base filter on 2026-07-26.
- Reference basis: local .NET `System/Context.cs:8-13`.

## Findings

The protected constructor prevents direct ordinary construction and permits a
concrete derived type, matching the usable C++ shape of .NET's abstract
`ContextBoundObject`. The header candidly declares its context/remoting
behavior a marker-only adaptation; current .NET itself has no active remoting
implementation here.

Its inherited base diverges: `MarshalByRefObject` is directly constructible in
this C++ surface and lacks the .NET legacy members, as recorded by SR-AUD-128.
That defect belongs to the base header rather than this derived marker.

## Other missing assertions and diagnostics

- Tests cover only construction, RTTI, and virtual deletion; no test records
  an unavailable context boundary, marshal failure, or cross-domain behavior.
- No compile-time assertion records that direct ContextBoundObject construction
  is inaccessible while an ordinary derived type is constructible.
- No diagnostic explains the exact supported replacement for the documented
  unimplemented context semantics.

## Final assessment

Sound marker adaptation subject to its base-class contract gap. No source or
test was modified during this audit.
