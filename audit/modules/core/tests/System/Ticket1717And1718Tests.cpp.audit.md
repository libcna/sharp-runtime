# Audit: `modules/core/tests/System/Ticket1717And1718Tests.cpp`

## Metadata

- AUDITED: 21 focused regressions for integral NumberStyles parsing and
  collection/StringBuilder `CopyTo` validation.
- Validation: the complete Core.Base fixture passed 4,946/4,946.

## Assessment

The tests retain useful hexadecimal, whitespace, overflow, null destination,
capacity, and index diagnostics.  They sample each integral type but do not
exercise exponent styles, incompatible/unknown NumberStyles masks, or the
broader culture/UTF-8 parsing matrix that has already exposed parser contract
gaps elsewhere.

## Other missing assertions and diagnostics

- Add valid `AllowExponent`, unknown/incompatible style masks, trailing junk,
  exact out-parameter preservation, and culture-provider cases.
- Add aliasing/overlap, empty source, zero count, extreme indexes, and
  nontrivial element cases for both `List::CopyTo` and `StringBuilder::CopyTo`.
- Keep each historical ticket's acceptance criteria adjacent to its regression
  so generic tests do not obscure why a boundary is protected.

## Final assessment

The historical regressions pass but leave important parser and copy boundaries
unasserted. No source or test was changed.
