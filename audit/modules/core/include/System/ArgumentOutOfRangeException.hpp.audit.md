# Audit: `modules/core/include/System/ArgumentOutOfRangeException.hpp`

## Metadata

- Audit status: AUDITED (245-line public declaration/templates, fully read).
- Validation: the three-fixture argument-exception filter passed 64/64 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Compile reproducer: compiling
  `/tmp/sharp-runtimervc-argumentoutofrange-generic-probe.cpp` fails in
  `ThrowIfGreaterThan(OrderedOnly{1}, OrderedOnly{0}, "value")` because
  `std::to_string(OrderedOnly)` has no overload.
- Reference: local .NET `ArgumentOutOfRangeException.cs` leaves
  `ThrowIfEqual<T>` unconstrained and constrains ordering helpers only to
  `IComparable<T>`; it does not require a formatting interface to compare.

## Assessment

The declaration covers the principal constructor and static guard shape, and
the actual-value constructor has a documented message-order policy.  Every
template guard nevertheless formats its values with `std::to_string`, adding a
hidden arithmetic-only constraint beyond the declared equality/ordering
requirements.

## SR-AUD-091 — medium — generic ArgumentOutOfRange guards silently require `std::to_string` in addition to their declared comparison contract

`ThrowIfGreaterThan`, `ThrowIfGreaterThanOrEqual`, `ThrowIfLessThan`, and
their equality counterparts describe `T` as totally ordered or
equality-comparable.  Their throwing branches instantiate `std::to_string`
for `value` and sometimes `other`; C++ instantiates that expression with the
function template, so even an `OrderedOnly` type with all required comparison
operators cannot call the API.  The reproducer fails at the undeclared
formatter, not at a clear public constraint diagnostic.

Current .NET likewise permits arbitrary equality-comparable `T` for
`ThrowIfEqual` and only comparison-capable `T` for ordering helpers.  A C++
adaptation may intentionally narrow the contract, but must declare and
diagnose the formatting requirement or format optional actual values through a
separate policy.

## Other missing assertions and diagnostics

- All tests use integers plus one `double`; none compiles a comparison-only,
  equality-only, enum, `std::string`, custom formatter, NaN, signed-minimum,
  or unsigned-maximum input.
- The template comments promise generic ordering but provide no concept,
  `static_assert`, or diagnostic naming `std::to_string` as the extra
  requirement.
- No test checks `actualValue`/message formatting for floating special values,
  exact parameter suffix order on every helper, empty parameter names, or
  comparisons with culturally formatted values.

## Final assessment

Numeric calls pass, but the advertised generic guard surface has a confirmed
compile-time restriction that is neither documented nor intentional.  No
source or test was modified during this audit.
