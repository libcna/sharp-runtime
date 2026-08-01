# Audit: `modules/threading/include/System/Threading/ThreadStartException.hpp`

## Metadata

- AUDITED: 37-line Thread-start exception declaration, fully read.
- Validation: complete Threading tests passed 359/359 on 2026-07-27; an mcs
  managed accessibility probe was compiled.
- Related fixture: audited Batch8 directly constructs every exposed C++ form.

## SR-AUD-196 — medium — ThreadStartException exposes runtime-internal constructors as ordinary public API

The header documents that .NET constructors are internal but publishes default,
message, and message-plus-inner constructors.  Batch8 constructs all three.
The managed `new ThreadStartException()` probe fails at compile time with
CS0122 (constructor inaccessible).  No production source creates this type,
so the C++ API is an inspectable user-created exception rather than a runtime
failure detail.

## Final assessment

SR-AUD-196 is confirmed by the local managed compiler baseline.  No source or
test was changed.

## Post-audit remediation — ticket #1875 (2026-08-01)

Every exposed constructor now assigns current .NET's `COR_E_THREADSTART`
(`0x80131525`) instead of `COR_E_SYSTEM`. SR-AUD-196 remains confirmed: no
constructor accessibility or public signature changed.
