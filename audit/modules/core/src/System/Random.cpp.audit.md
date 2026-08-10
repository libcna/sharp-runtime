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

**REMEDIATED — ticket #1902, 2026-07-31.** The design chosen is thread-local
backing generators behind an unchanged shared object, which is what .NET itself
does (`Random.cs:752–773`). `getSharedProperty()` still returns one `static
Random` with a **stable address on every thread** — the per-thread-*instance*
shortcut is the one .NET rejects explicitly at `Random.cs:755–759`, because a
caller may obtain `Shared` on one thread and use it on another — and
`internalSample()`, the only method that writes `seedArray_`/`inext_`/`inextp_`
and the funnel every entry point on the class passes through, routes the draw to
the calling thread's own generator when `this` is the shared instance. After
construction the shared object is never written again.

The check is a pointer comparison inside this file, not a member: `Random`'s
state lives in the public header, so a `bool isShared_` would have been an
object-layout change. **No header was touched**, `sizeof(Random)`/`alignof` are
unchanged at 240/8 and asserted in the suite, and `nm --extern-only` over this
translation unit before and after is **identical** at 58 external symbols.

The 8-thread TSan probe reported **6** races here before the repair
(`internalSample()` lines 72, 78, 83, 84, 85) and is **clean** after. A seeded
`Random(seed)` stream is byte-identical to the pre-repair one across 4,928
dumped values covering every entry point. +8 tests, ASan/UBSan/LSan clean,
mutation-checked — including against the .NET-rejected shortcut, which is
race-free and so invisible to TSan but fails
`Shared_ReferenceIsTheSameAddressOnEveryThread`. Full evidence:
`docs/SharedPrngConcurrencyPlan.md` §11–§16.

The `protected Sample()` seam concern raised in the plan did **not** materialise:
a derived `Random` can never be the shared instance, so derived types are
untouched, and the recorded mutex fallback was not needed.

## Positive findings

The seeded algorithm has direct cross-runtime parity vectors for several
seeds, bounds, doubles, and bytes.  `NextInt64` range arithmetic avoids signed
overflow by working in unsigned space.

## Final assessment

The single-threaded PRNG implementation is carefully ported; its shared API
has a high-severity concurrency contract breach (SR-AUD-010).
