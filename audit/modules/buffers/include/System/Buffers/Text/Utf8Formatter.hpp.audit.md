# Audit: `modules/buffers/include/System/Buffers/Text/Utf8Formatter.hpp`

## Metadata

- Audit status: AUDITED (297-line public header-only implementation, fully
  read).
- Validation: `Utf8FormatterTest.*` passed 25/25 in
  `SharpRuntimeTests_Buffers` on 2026-07-26.
- Reference: local .NET `Utf8Formatter.Boolean.cs`, `Utf8Formatter.Integer.cs`,
  and `FormattingHelpers.cs` plus the formatter validation source were reviewed.

## Assessment

Within the explicitly implemented bool and 8/16/32/64-bit integer subset, the
header uses safe unsigned magnitude conversion for signed minima, masks signed
hex output to the original width, checks destination capacity before copying,
and distinguishes invalid format from unsupported G/R precision.  The
high-precision stack-buffer regression has already been covered by four direct
tests and the current 100/101/150-byte staging buffers correctly accommodate
the public 0–99 precision range.  No new evidence-backed implementation defect
was found in that subset.

## Other missing assertions and diagnostics

- The direct suite covers only `uint8_t`, `int32_t`, `uint32_t`, `int64_t`, and
  `uint64_t`.  It omits both extrema and ordinary values for `int8_t`,
  `uint16_t`, and `int16_t`, and omits the signed-minimum paths for all signed
  widths.
- No test uses lowercase `d`/`n`, uppercase `G`/`R`, no-precision explicit
  `StandardFormat`, bool with precision, or `R` success.  It also does not
  assert bytesWritten/output preservation after throwing format requests.
- Short-buffer coverage checks only bool true and one integer.  It omits exact
  boundaries and high-precision D/X/N failures, nonmutation sentinels,
  pre-populated `bytesWritten`, negative grouped output, and final byte counts
  for each type width.
- There is no differential corpus for full 0–99 D/X/N precision, all signed
  bit patterns, grouping boundaries, or C++ locale independence.  The
  implementation uses literal `,` and `.` as current .NET invariant UTF-8
  formatting does, but this is not independently protected by tests.
- Public .NET overloads for decimal, float/double, Guid, DateTime,
  DateTimeOffset, and TimeSpan remain absent.  The header explicitly documents
  this API gap, so it remains an implementation-roadmap issue rather than a
  newly classified silent defect.
- The `StandardFormat` default/zero-symbol `ToString` defect SR-AUD-083 is not
  reached by format dispatch because this header uses `IsDefault` directly;
  integration tests should nevertheless verify that a parsed/default format is
  never converted to its faulty textual form before being passed here.

## Final assessment

The audited bool/integer subset has no newly confirmed defect.  Its breadth
and failure-output diagnostics remain materially under-tested.  No production
or test source was modified during this audit.
