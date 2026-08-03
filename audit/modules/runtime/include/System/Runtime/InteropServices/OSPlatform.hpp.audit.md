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
