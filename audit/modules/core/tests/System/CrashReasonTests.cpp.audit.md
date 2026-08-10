# Audit: `modules/core/tests/System/CrashReasonTests.cpp`

## Metadata

- AUDITED: 28-line dedicated fixture, fully read.
- Validation: `CrashReasonTests.*` passed 5/5 in the combined 17-test
  `CrashReasonTests.*:DateTimeKindTests.*:DayOfWeekTests.*` Core.Base filter
  on 2026-07-26.

## Findings

The fixture verifies all four copied numeric values and two inequalities.  By
including a standalone public header and treating it as ordinary public API,
it locks in the visibility/nesting drift documented in SR-AUD-127 instead of
testing an actual NativeAOT crash-information consumer.

## Missing assertions and diagnostics

- Missing full distinctness, invalid cast, switch, diagnostic serialization,
  and native fail-fast mapping vectors.
- No test documents that the .NET reference is internal and nested rather than
  a public `System.CrashReason` contract.
- No first-party production user exists, so the fixture cannot establish the
  claimed crash-diagnostics integration behavior.

## Final assessment

Complete numeric smoke coverage for the exposed declaration, but it preserves
SR-AUD-127's public-surface mismatch. No source or test was modified during
this audit.
