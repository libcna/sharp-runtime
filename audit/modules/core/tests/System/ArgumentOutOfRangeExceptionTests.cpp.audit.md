# Audit: `modules/core/tests/System/ArgumentOutOfRangeExceptionTests.cpp`

## Metadata

- Audit status: AUDITED (283 lines, forty tests, fully read).
- Validation: `ArgumentOutOfRangeExceptionTests.*` passed 40/40 within the
  64/64 argument-exception filter in `SharpRuntimeTests_Core_Base` on
  2026-07-26.

## Assessment

The fixture covers the constructor family, HResult, message suffix order, and
true/false boundaries for every published numeric guard.  It is strong for
`int` happy paths but treats templates as integer-only despite their broader
declarations.

## Finding references

- **SR-AUD-091:** every guard test instantiates numeric types accepted by
  `std::to_string`; no equality-only or ordered-only user type compiles the
  advertised generic API, so the hidden formatter constraint remains unseen.
  **REMEDIATED (#2254, 2026-08-10).** This fixture is unchanged; the generic
  coverage lives in the new sibling
  `modules/core/tests/System/ArgumentOutOfRangeGuardDomainTests.cpp` (26 tests, one
  per formatter branch plus the byte-identical arithmetic regression pins), and the
  compile-time half — which no run-time fixture can express — in
  `test/consumer/core_argument_out_of_range_guard_domain_negative.cpp` (6 sites) and
  `test/consumer/core_base.cpp`. This report's own recommendation that "failure
  diagnostics should name the supported C++ contract rather than leak a
  standard-library overload set" is exactly what was implemented.

## Other missing assertions and diagnostics

- Add a compile-only comparison-only/equality-only type, `std::string`, enum,
  and a deliberately unformattable type; failure diagnostics should name the
  supported C++ contract rather than leak a standard-library overload set.
- The suite omits NaN, infinities, signed minima, unsigned maxima, `-0`, empty
  parameter names, and exact messages for all helper forms.
- No test asserts typed actual-value behavior, inner-exception identity,
  copies/moves, or all constructors' HResults and suffix ordering.

## Final assessment

All forty numeric tests pass, but their scalar-only instantiation leaves a
confirmed generic compile-time compatibility gap untested.  No source or test
was modified during this audit.
