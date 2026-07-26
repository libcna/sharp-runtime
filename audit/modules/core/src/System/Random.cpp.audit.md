# Audit: `modules/core/src/System/Random.cpp`

## Metadata

- Audit status: AUDITED (277 lines, full read).
- Implementation: .NET-compatible subtractive PRNG, range sampling, bytes,
  string helpers, and process-wide shared instance.
- Validation: focused Random/OperatingSystem filter passed 92 tests;
  deterministic seed parity tests are included in that run.

## Assessment

The generator takes notable care to express .NET's unchecked seed arithmetic
through defined C++ unsigned arithmetic.  Bounds and large signed ranges are
similarly handled with explicit widths.  The global shared instance is plain
mutable state with no synchronization.

## Findings

### SR-AUD-010 — high — `Random::Shared` is documented as thread-safe but has a C++ data race

`getSharedProperty` returns a single `static Random instance` (lines 244–248).
Every generation method mutates `seedArray_`, `inext_`, and `inextp_` through
ordinary non-atomic reads and writes (`internalSample`, lines 62–79).  There
is no mutex, atomics, locking wrapper, or per-thread generator.  C++11
guarantees only thread-safe *initialization* of the static object; it does not
make later mutation safe.

**Reproduction:** concurrently call
`Random::getSharedProperty().Next()` from two or more threads.  Those calls
race on the fields above, which is undefined behavior under the C++ memory
model.  The public header expressly promises “a thread-safe shared Random
instance usable from any call site,” and .NET `Random.Shared` is thread-safe.

**Impact:** normal concurrent users can encounter undefined behavior, lost
state updates, and non-deterministic corruption.  The issue is limited to the
global shared instance; separately owned `Random` objects make no thread-safe
promise.

`Guid::NewGuid` independently reproduces the same class of defect: its static
`std::mt19937_64` is called concurrently without synchronization, and the
2026-07-26 eight-thread TSan probe reports a race at `Guid.cpp:344`.  See
`audit/modules/core/src/System/Guid.cpp.audit.md`.  The two APIs need separate
ownership/remediation choices, but both prove that mutable process-wide PRNG
state is not made safe merely by static initialization.

**Required post-audit verification:** select a design that preserves the
public reference API (for example a mutex-protected facade or thread-local
backing generators), then add a multi-thread stress test and a focused TSan
run.  Do not merely remove the documentation claim without an API decision.

## Positive findings

The seeded algorithm has direct cross-runtime parity vectors for several
seeds, bounds, doubles, and bytes.  `NextInt64` range arithmetic avoids signed
overflow by working in unsigned space.

## Final assessment

The single-threaded PRNG implementation is carefully ported; its shared API
has a high-severity concurrency contract breach (SR-AUD-010).
