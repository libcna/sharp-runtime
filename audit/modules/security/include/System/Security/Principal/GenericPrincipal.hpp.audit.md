# Audit: `modules/security/include/System/Security/Principal/GenericPrincipal.hpp`

## Metadata

- AUDITED: identity validation/ownership, role-list copy, case-insensitive
  membership, and reduced claims scope.
- Validation: the security fixture passed 38/38.  A direct native/current-.NET
  probe used role `ÄDMIN` and query `ädmin`; C++ returned false and current
  .NET returned true.

## SR-AUD-246 — medium — role matching reduces managed Unicode OrdinalIgnoreCase semantics to bytewise ASCII lowercasing

`equalsIgnoreCase` invokes `std::tolower` separately on UTF-8 bytes.  It
therefore cannot case-fold non-ASCII role names: the direct C++ probe reports
false for `ÄDMIN`/`ädmin`, whereas current .NET `GenericPrincipal.IsInRole`
uses `StringComparison.OrdinalIgnoreCase` and reports true.  This is an
observable authorization-membership mismatch, not a spelling-only adaptation.

## Assessment

The constructor correctly rejects an empty shared identity and copies the
provided role vector.  The documented standalone design intentionally omits
the managed claims fallback; SR-AUD-246 is independent of that declared scope
because it affects roles explicitly supplied to this implementation.

## Other missing assertions and diagnostics

- Add the direct non-ASCII case pair above, other Unicode simple-case pairs,
  empty role input, and a regression that the source role vector is copied.
- State the selected Unicode case-folding policy in diagnostics; ASCII-only
  behavior must not remain implicit in a principal API.

## Final assessment

SR-AUD-246 is reproduced for explicit Unicode role names. No source or test
was changed during this audit.
