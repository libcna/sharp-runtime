# Audit: `modules/core/tests/System/TryWriteInterpolatedStringHandlerTests.cpp`

## Metadata

- AUDITED: 123-line dedicated fixture, fully read.
- Validation: `TryWriteInterpolatedStringHandlerTests.*` passed 13/13 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Findings

The fixture provides normal construction, literal, failure-latch, and simple
formatted-value smoke coverage. It never supplies an invalid raw pointer, so
it misses the ASan-confirmed null write in SR-AUD-132. It also asserts the
wrong C++ bool spelling `"1"` and merely asserts that a formatted `255` is
nonempty rather than requiring the .NET `X2` result `"FF"`, preserving
SR-AUD-133.

## Missing assertions and diagnostics

- Missing null destination/literal, zero-length, exact-capacity, overlap,
  embedded-NUL, and failure-prefix tests.
- Missing exact bool, integer base/precision, floating general/precision,
  alignment, provider, enum, custom-formattable, and Unicode output vectors.
- No test covers the compiler-handler/MemoryExtensions.TryWrite integration,
  output-count reset on final failure, copying/escaping, or concurrent use.
- The test buffers are uninitialized yet only string lengths are observed;
  no sentinel check proves no write occurs beyond destination capacity.

## Final assessment

Useful success/failure smoke coverage, but it locks in both confirmed handler
defects. No source or test was modified during this audit.
