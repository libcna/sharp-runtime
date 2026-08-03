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

## Post-audit disposition — the `System::Runtime` namespace review, ticket #1972 (2026-08-03)

This file's open finding(s) were re-verified against current source on 2026-08-03 and
remain **confirmed** with no correction to premise, count, severity or consequence.
They belong to cause **R-G** — *the public shape itself diverges* — the runtime
analogue of `docs/ThreadingNamespaceReviewPlan.md`'s cause **T-H**, and are owned by
the **approval-gated** ticket **#1980**. Each one changes a declaration a consumer can
already name (a base class, a sealing decision, a constructor set, a public enum value,
a field default, a field type, or a property's presence), so none is implemented
without approval.

`docs/SystemRuntimeNamespaceReviewPlan.md` §10.2 splits R-G into five groups so the
answer can be partial, and recommends **G-1** — `OSPlatform`'s default constructor,
`RuntimeInformation`'s two absent identity properties, and `ExternalException`'s
error-code surface — as the cheapest minimum, because it is **purely additive** and
cannot break a consumer.

**No new `SR-AUD-*` identifier was issued**; numbering stays frozen at **364**.
