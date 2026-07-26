# Audit: `modules/core/src/System/ArgumentException.cpp`

## Metadata

- Audit status: AUDITED (80-line implementation, fully read).
- Validation: the three-fixture argument-exception filter passed 64/64 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-argumentexception-audit-probe.cpp`
  prints `nbsp-accepted` after `ThrowIfNullOrWhiteSpace("\\xC2\\xA0")`.

## Assessment

Constructor paths consistently set `COR_E_ARGUMENT`, preserve the supplied
parameter property, and centralize suffix construction.  `all_of` with the
unsigned-byte `std::isspace` idiom is safe from negative-char UB but is not a
Unicode whitespace predicate for the documented UTF-8 string boundary.

## Finding references

- **SR-AUD-048 (extended):** the byte-only whitespace predicate accepts the
  UTF-8 encoding of U+00A0.  Current .NET rejects that value as whitespace.
  This is an observable validation bypass, not merely a locale spelling
  difference.

## Other missing assertions and diagnostics

- The test suite omits all non-ASCII whitespace and does not record the active
  locale, code-page assumptions, malformed sequence policy, or source
  character indexing.
- No constructor test asserts exact default message/suffix composition,
  combines a parameter with an inner exception, or exercises embedded NUL and
  non-ASCII parameter names.
- `std::exception_ptr` is stored only by the base; no test exposes a public
  inner-exception accessor or defines the diagnostic policy for a null pointer.

## Final assessment

The ASCII validation path works, but UTF-8 whitespace is silently treated as
ordinary input.  No source or test was modified during this audit.
