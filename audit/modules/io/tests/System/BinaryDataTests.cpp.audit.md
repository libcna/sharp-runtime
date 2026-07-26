# Audit: `modules/io/tests/System/BinaryDataTests.cpp`

## Metadata

- AUDITED: 116-line direct IO BinaryData fixture, fully read.
- Validation: `BinaryDataTests.*` passed 15/15 in `SharpRuntimeTests_IO` on
  2026-07-27. It is a different executable from the Core fixture that uses
  the same suite name.
- Related implementation evidence: audited BinaryData report, including
  SR-AUD-185/SR-AUD-186; hash assertion also extends SR-AUD-018.

## Assessment

The fixture covers content-based C++ equality/hash adaptation, stream and
read-only stream behavior, index diagnostics, and borrowed-view conversion.
Normal behavior passes. It also fixes a known post-repair negative-index
regression. No new implementation defect is demonstrated beyond the owning
BinaryData report.

## Finding reference: SR-AUD-018 extension — the hash test forbids a valid negative hash code

`GetHashCode_NonNegative` requires `BinaryData::GetHashCode()` to be
nonnegative. A hash API's signed result may legally be negative; only equal
values require equal hashes. The current implementation masks its result
positive, so this test passes while overconstraining a valid replacement. This
extends the existing low-severity test-contract finding SR-AUD-018.

## Other missing assertions and diagnostics

- Equality tests lock in the documented native content-equality adaptation but
  omit collision behavior, large data, empty/media-type null-like semantics,
  and the managed identity distinction.
- Tests omit invalid UTF-8 decoding/replacement (SR-AUD-185) and mutation
  after ReadOnlyMemory construction (SR-AUD-186).
- File conversions, stream error/partial/large input, ToMemory/Span lifetime,
  implicit view after BinaryData destruction/reallocation, and async/JSON
  omission diagnostics are untested.

## Final assessment

The fixture protects several normal IO paths and index bounds, but contains
the SR-AUD-018 hash-sign assertion and misses both new BinaryData findings. No
source or test change.
