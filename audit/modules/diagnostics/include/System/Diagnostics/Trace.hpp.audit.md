# Audit: `modules/diagnostics/include/System/Diagnostics/Trace.hpp`

## Metadata

- AUDITED: stderr trace output, conditional writes, assertions, and indentation.
- Evidence: declaration review and 35 Trace/conditional-write tests in the target.

## Assessment

The supported output and false-condition suppression paths pass. The global
indent-size storage is unsynchronised, like Debug's global provider/indent
state; SR-AUD-275 records the confirmed shared-static diagnostics race family.

## Other missing assertions and diagnostics

- Add thread-safe line-atomic output/indent tests, concurrent setter coverage,
  and stderr failure diagnostics under multiple writers.

## Final assessment

SR-AUD-275 applies to the shared diagnostics-state pattern. No source or test changed.
