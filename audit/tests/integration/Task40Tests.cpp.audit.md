# Audit: `tests/integration/Task40Tests.cpp`

## Metadata

- Audit status: AUDITED (1,478 lines, 236 tests in 22 suites, full read).
- Runtime evidence: the focused span, numeric, date/time, formatting, platform,
  comparer, progress, Unicode, cancellation, exception, and helper filter
  passed all 236 cases on 2026-07-25.

## Coverage observed

This large aggregate file contains strong focused regressions for `Span` bounds
overflow, `Int128` defined wrapping/overflow behavior, `DateTimeOffset`
RFC/UTC formatting and out-of-range offset handling, `BFloat16` special values,
and `Rune`-adjacent Unicode range basics.  The `Int128` tests deliberately
exercise code paths that are undefined with raw signed `__int128` arithmetic;
they are meaningful native-port safety evidence rather than superficial value
tests.

## Relation to confirmed findings

- The `DateTimeOffset` cases reject only `+15:00`; they do not test invalid
  component hours inherited from `DateTime` or syntactically accepted impossible
  offset minutes such as `+02:75`.  Thus they do not detect **SR-AUD-006** or
  the offset portion of **SR-AUD-007**.
- The six `TimeOnly` cases cover construction/formatting only.  They do not
  call `TryParse`, including malformed fractional suffixes, and cannot detect
  **SR-AUD-009**.
- The FormattableString and factory cases cover only sequential ordinary
  placeholders.  They do not exercise escaped/malformed braces, missing
  indices, argument text containing brace syntax, or empty factory format; they
  therefore cannot detect the FormattableString extension of **SR-AUD-015** or
  the contradictory factory documentation in **SR-AUD-059**.

## Missing assertions and diagnostics

- `UInt128` has only seven happy-path tests, compared with 68 `Int128` cases.
  It lacks parse/format precision, shifts/rotates, divide/modulo by zero,
  overflow wrapping, upper-word boundaries, and format/exception diagnostics.
- `Half` and `BFloat16` lack subnormal, rounding-tie, overflow, signed-zero
  formatting, and payload-sensitive NaN tests.  Approximate comparisons alone
  do not lock binary conversion semantics.
- `ReadOnlySpan` lacks bounds, negative/overflowing slice, copy, and aliasing
  tests.  `Span` has good bounds coverage, but it is not proof that its
  independent read-only paths use the same checks.
- Platform predicates are asserted mainly for this Linux host and documented
  false stubs.  They need platform-runner evidence before being treated as
  Windows/Apple/WASI compatibility validation.
- Cancellation registration is exercised only in its default inactive state;
  no source/token callback is registered, invoked, disposed, or raced.
- `Progress<T>` checks direct handler calls but not its context-dispatch,
  exception, reentrancy, lifetime, or concurrency behavior.

## Required post-audit verification

Preserve the existing span/`Int128` regression vectors.  Add negative parser
and invalid component/offset tests alongside the already indexed DateTime
findings, then address UInt128 and numeric boundary vectors under UBSan.  Test
platform and cancellation semantics on the relevant runner/API paths rather
than inferring them from a default-object smoke test.

## Final assessment

Despite its aggregate shape, this is a valuable source of arithmetic and
formatting regressions.  It confirms test omissions related to existing
date/time findings but does not demonstrate a distinct new source defect.
