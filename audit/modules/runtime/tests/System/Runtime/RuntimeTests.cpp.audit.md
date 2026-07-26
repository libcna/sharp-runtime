# Audit: `modules/runtime/tests/System/Runtime/RuntimeTests.cpp`

## Metadata

- AUDITED: 659-line shared fixture, fully read.
- Validation: the exact 32-suite filter executed all 82 declared tests and
  passed 82/82 on 2026-07-27.
- Scope: compiler-services metadata, ConditionalWeakTable, RuntimeHelpers,
  caller attributes, GCSettings, selected interop attributes/exceptions, and
  versioning-attribute smoke coverage.

## Assessment

The fixture has useful direct state assertions for ConditionalWeakTable
identity, expiry/snapshot behavior, one concurrent GetOrAdd path, RuntimeHelpers
range/exception behavior, and the represented metadata fields.  Its 82 tests
are concentrated normal-path smoke checks rather than an exhaustive contract
suite; individual component reports own source-level verdicts.

The GCSettings segment proves only that valid writes round-trip.  It omits
every invalid enum boundary, leaving the confirmed setter-validation failure
in [SR-AUD-156](../../../../include/System/Runtime/GCSettings.hpp.audit.md#sr-aud-156--medium--gcsettings-setters-retain-values-outside-their-public-enum-domains)
unguarded.

Its AmbiguousImplementationException and ExternalException segments likewise
assert normal text/throwability only.  They do not cover the confirmed derived
HResult failure in SR-AUD-157, Ambiguous's wrong catch hierarchy/missing cause
constructor (SR-AUD-158), or ExternalException's absent error-code route
(SR-AUD-159).

## Other missing assertions and diagnostics

- GCSettings omits `SustainedLowLatency`, invalid latency/LOH casts, attempted
  `NoGCRegion` assignment, and any observable interaction with the declared
  RAII GC boundary.
- The three caller-info attribute tests only instantiate marker objects.  They
  have no compile-time call-site expansion assertion, file/line diagnostic, or
  negative attachment case.
- ConditionalWeakTable concurrency checks equal returned values but not
  factory-call cardinality under racing factories, exception propagation,
  null values, or enumeration after removal; the one thread schedule is not a
  stress diagnostic.
- Interop and versioning coverage samples a few values/default fields.  It
  omits invalid enums, all remaining fields/constructors, and declaration-level
  compiler/interop effects that ordinary C++ object construction cannot prove.
- Marker-attribute tests end in `SUCCEED()` after construction, so they do not
  diagnose whether a declaration actually carries the intended metadata.
- The two exception segments omit every HResult, derived-type catch boundary,
  inner-cause rethrow/identity, and ExternalException ErrorCode/hex-format
  assertion; their green normal-message tests therefore cannot detect
  SR-AUD-157 through SR-AUD-159.

## Final assessment

The complete aggregate fixture is green and provides useful representative
coverage, but its GC segment preserves the normal-path-only gap behind
SR-AUD-156.  No source or test was modified.
