# Audit: `modules/core/include/System/LoaderOptimizationAttribute.hpp`

## Metadata

- Audit status: AUDITED (49-line value attribute, fully read).
- Validation: `LoaderOptimizationAttributeTests.*` passed 12/12 in the
  77-test focused Core.Base attribute filter on 2026-07-26.
- Reference basis: local .NET `System/LoaderOptimizationAttribute.cs:6-19`.

## Findings

Both constructors preserve the byte/enum payload and the getter returns the
same `LoaderOptimization` value.  The C++ port cannot attach the object to a
method, so it has no loader effect; modern .NET's multi-domain choices are
legacy as well.

## Other missing assertions and diagnostics

- Tests cover values zero through four but omit arbitrary byte values and the
  `DomainMask` alias through the attribute constructor.
- The `AttributeTargets.Method` restriction, sealed type policy, and metadata
  retrieval cannot be exercised without the intentionally excluded reflection
  facility.

## Final assessment

Payload storage is compatible and the absent metadata effect is a documented
runtime limitation.  No source or test was modified during this audit.
