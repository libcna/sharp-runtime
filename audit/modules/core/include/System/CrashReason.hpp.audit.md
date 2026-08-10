# Audit: `modules/core/include/System/CrashReason.hpp`

## Metadata

- AUDITED: 26-line public enum declaration, fully read.
- Validation: `CrashReasonTests.*` passed 5/5 in the combined 17-test
  `CrashReasonTests.*:DateTimeKindTests.*:DayOfWeekTests.*` Core.Base filter
  on 2026-07-26.
- Reference basis: local NativeAOT
  `System/CrashInfo.cs:13-22,116-124`.

## SR-AUD-127 — medium — CrashReason publishes a private nested NativeAOT implementation enum as a public System API

The counterpart in current .NET is `internal` and nested as
`System.CrashInfo.CrashReason`; it is only used to encode NativeAOT triage
data.  This header instead publishes an independently includable
`System::CrashReason` in the Core.Base public include tree.  Repository search
finds no first-party production consumer — only the dedicated test — so the
public surface has no current runtime role to justify the visibility or lost
nesting.

The numerical values happen to match, but the public name reserves a `System`
identifier for an implementation detail and promises stable consumer access
where .NET intentionally makes none.  Move it into an internal NativeAOT
crash-diagnostic implementation or explicitly designate/document it as a
project-specific public diagnostic type rather than calling it a .NET
counterpart.

## Other missing assertions and diagnostics

- No production crash/FailFast path consumes the enum, so no test validates
  serialization into a triage record or mapping from native failure reasons.
- The header provides no visibility or NativeAOT-only diagnostic explaining
  why an internal runtime concept is exposed from Core.Base.
- No invalid-value or full pairwise-distinctness vector exists.

## Final assessment

The constant values match the internal reference but its public placement is
the confirmed SR-AUD-127 contract drift. No source or test was modified during
this audit.
