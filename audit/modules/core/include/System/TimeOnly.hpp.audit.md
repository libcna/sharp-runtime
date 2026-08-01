# Audit: `modules/core/include/System/TimeOnly.hpp`

## Metadata

- Audit status: AUDITED (426 lines, full read).
- Public API: partial, millisecond-precision C++ counterpart of
  `System.TimeOnly`.

## Assessment

The header explicitly documents its millisecond precision and additive missing
surface, while construction, circular arithmetic, and range validation are
straightforward.  Its fixed accepted parser grammar is more restrictive than
.NET by design, but it still requires malformed input to return false.

## Finding reference

The implementation does not enforce complete consumption of that stated
grammar.  See **SR-AUD-009** in
[`TimeOnly.cpp.audit.md`](../../src/System/TimeOnly.cpp.audit.md).

## Final assessment

The intentional precision/API limitations are clear.  The supported parser
needs strict-input remediation and tests (SR-AUD-009).

---

## Correction — the public ticks contract was not millisecond precision (2026-08-01)

The audit's `millisecond-precision` characterization is preserved above. It
matched the old storage, but conflicted with the same header's public ticks
constructor/property contract and caused silent sub-millisecond truncation.
Ticket #1929's exact row-6 approval requires TimeOnly fractions through seven
digits at 100-nanosecond resolution. The fourth existing `int` now stores
ticks-within-the-second, so the object remains four ints (`sizeof=16`,
`alignof=4`) while its conversions/operators/formatting honor that contract.
The inseparable defect is inactive post-audit ticket #1931, completed without
changing SR-AUD-009 or issuing a new finding number.
