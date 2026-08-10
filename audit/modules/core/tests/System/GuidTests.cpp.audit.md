# Audit: `modules/core/tests/System/GuidTests.cpp`

## Metadata

- Audit status: AUDITED (612 lines, 80 tests, full read).
- Validation: `GuidTests.*` passed 80/80 in `SharpRuntimeTests_Core_Base` on
  2026-07-26.

## Assessment

The suite covers empty values, all standard display forms, byte order,
constructors, equality/comparison, fixed-format parsing, buffer-capacity
failures, and a valuable set of X-format overflow/precedence regressions.  Its
sequential `NewGuid` checks establish only UUID shape and a tiny probabilistic
sample.  They leave the actual concurrent and strong-entropy contracts
unobserved, so the full suite is green while SR-AUD-010 and SR-AUD-050 remain
reachable.

## Finding references

- **SR-AUD-010:** `NewGuid*` and `CreateVersion7*` tests make one or two calls
  from one thread only.  No bounded multi-thread completion/range test or TSan
  execution exists, so the sanitizer-confirmed shared-engine race is invisible
  to this suite.
- **SR-AUD-043 (extended):** only the 16-byte `ReadOnlySpan` constructor's
  ordinary wrong-length error is tested.  There are no `Parse`/`TryParse`
  tests for invalid char or UTF-8 span metadata before those overloads cast it
  to an unsigned string length.
- **SR-AUD-050:** version/variant assertions and a 20-value uniqueness loop do
  not distinguish a per-call OS CSPRNG from a seeded Mersenne Twister.  No
  injectable random-source seam or failure-path diagnostic exists.

## Required post-audit verification

After the source repair, add a bounded parallel factory test that checks
completion, non-empty structural UUIDs, and v4/v7 bits without making a
probabilistic uniqueness claim the sole oracle; run it under TSan.  Test the
secure-byte provider behind a deterministic seam for failure propagation and
exactly sixteen-byte requests.  Add sanitizer-backed malformed-span parsing
checks after the common Span invariant is repaired.

## Other missing assertions and diagnostics

- `CreateVersion7(DateTimeOffset)` lacks exact timestamp-byte assertions at
  the Unix epoch, a known millisecond value, and the pre-epoch rejection
  boundary.
- Parse tests do not cover the header-documented Unicode-whitespace deviation,
  so its deliberate compatibility boundary is not executable documentation.
- `TryFormat`/`TryFormatUtf8` cover fit/short capacity but not each output form
  or `charsWritten`/`bytesWritten` after a failed write.

## Final assessment

The 80 tests are useful ordinary compatibility coverage, especially for X
format parsing.  They omit the concurrency, secure-randomness, malformed-span,
and v7 timestamp boundaries that matter for the confirmed findings.  No test
was modified during this audit.
