# Audit: `modules/runtime/include/System/Runtime/CompilerServices/CompilerFeatureRequiredAttribute.hpp`

## Metadata

- AUDITED: 44-line inline value-metadata declaration, fully read.
- Validation: `CompilerFeatureRequiredAttributeTests.*` passed 1/1 on
  2026-07-27; the compiler-services shared filter passed 9/9.
- Reference basis: local current-.NET `CompilerFeatureRequiredAttribute.cs`.

## SR-AUD-160 — low — CompilerFeatureRequiredAttribute makes the init-only IsOptional property freely mutable

Current .NET declares `IsOptional` as `init`, so it can be supplied only while
an attribute instance is initialized.  C++ exposes an unrestricted
`setIsOptionalProperty` member, allowing arbitrary post-construction mutation.
The only direct test performs exactly that mutation after construction and
asserts the new state, preserving the broader native state transition rather
than the managed initialization contract.

The attribute is documented as compiler-unconsumed in this port, limiting the
immediate impact to public metadata/value semantics; it is nevertheless an
observable API difference for code that retains and passes the object.

## Assessment

The feature-name constructor, default false value, and `RefStructs`/
`RequiredMembers` constants agree with current .NET.  The documented absence
of native compiler consumption is an intentional capability boundary, not a
separate finding.

## Other missing assertions and diagnostics

- The fixture checks only one custom token, both constants, and the mutable
  transition; it omits initial-state stability, empty/UTF-8 feature names, and
  a construction-time-only representation of `IsOptional`.
- No compile-time fixture shows that native compilation ignores this object or
  reports an unknown feature, so callers have no executable diagnostic for the
  documented non-consumption boundary.

## Final assessment

The represented values are correct, but `IsOptional` is wider-mutable than its
managed init-only contract.  No source or test was modified.

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
