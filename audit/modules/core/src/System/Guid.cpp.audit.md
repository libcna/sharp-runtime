# Audit: `modules/core/src/System/Guid.cpp`

## Metadata

- Audit status: AUDITED (569 lines, full read).
- Validation: `build/SharpRuntimeTests_Core_Base --gtest_filter='GuidTests.*'`
  passed 80/80 on 2026-07-26.
- Independent concurrency probe:
  `/tmp/sharp-runtimervc-guid-race-audit-probe.cpp`, compiled with
  `-fsanitize=thread -fno-omit-frame-pointer` together with this translation
  unit on 2026-07-26.

## Assessment

The canonical-byte layout, endian conversions, component constructors,
formatters, and ordinary N/D/B/P/X parser paths are carefully structured.
Comparison is intentionally component/unsigned byte order and matches current
.NET's unsigned field comparisons.  The parser's documented compatibility
choices are explicit.  Three span overloads nevertheless inherit the already
confirmed invalid-length representation, and the two UUID constructors obtain
all random bytes from one unsafe, non-cryptographic global engine.

References: [.NET `Guid.NewGuid` contract](https://learn.microsoft.com/en-us/dotnet/api/system.guid.newguid?view=net-10.0) and [current .NET `Guid` source](https://source.dot.net/System.Private.CoreLib/src/libraries/System.Private.CoreLib/src/System/Guid.cs.html).

## Finding references

### SR-AUD-010 (extended) — high — `Guid::NewGuid` races on its static PRNG

Lines 339–348 create one static mutable `std::mt19937_64` and pass it to
`std::uniform_int_distribution` sixteen times per invocation.  C++ makes the
static's initialization safe, not later concurrent mutations.  `CreateVersion7`
(lines 351–377) calls `NewGuid`, therefore it has the same concurrent path.

**Reproduction:** build and run the bounded eight-thread, 50,000-calls-per-
thread probe:

```sh
c++ -std=c++20 -O1 -g -fsanitize=thread -fno-omit-frame-pointer \
  -I modules/core/include /tmp/sharp-runtimervc-guid-race-audit-probe.cpp \
  modules/core/src/System/Guid.cpp build/libsharp_runtime_core.a -pthread \
  -o /tmp/sharp-runtimervc-guid-race-audit-probe
TSAN_OPTIONS=halt_on_error=1 /tmp/sharp-runtimervc-guid-race-audit-probe
```

TSan reports a write/read race in
`std::mersenne_twister_engine::_M_gen_rand`, points to `Guid.cpp:344`, and
identifies `System::Guid::NewGuid()::rng` as the global location.  This is a
second independently reproduced instance of SR-AUD-010, not an inference from
the `Random::Shared` report.

**REMEDIATED — ticket #1901, 2026-07-31.** The engine and the distribution are
now per-thread, reached through a file-local `newGuidEngine()`. Nothing is handed
out to the caller here — unlike `Random::getSharedProperty()`, which must keep
one stable address — so per-thread state is an exactly equivalent repair rather
than a compromise. `CreateVersion7` inherits it through `NewGuid` with no edit of
its own, and its timestamp prefix comes from the clock, so it is unaffected.

Each thread's seed mixes a once-per-process `std::random_device` `base` with an
atomic counter through an odd multiplier, which is a bijection modulo 2^64 and so
guarantees **distinct** seeds even on a platform whose `random_device` is
deterministic — as MinGW-w64's historically was. Seeding from `random_device`
alone would have removed the race and replaced it with duplicate GUIDs; that
mutation is pinned and fails three tests.

The 8-thread TSan probe reported **13** races at `Guid.cpp:344` before and is
**clean** after. 100,000 concurrent `NewGuid()` across 8 threads produce zero
duplicates and zero nil values with correct version/variant nibbles; 16,000
concurrent `CreateVersion7()` likewise. `sizeof(Guid)`/`alignof` unchanged at
16/1 and asserted; `nm --extern-only` over this translation unit is **identical**
before and after at 87 external symbols; no header touched. +6 tests,
ASan/UBSan/LSan clean. Full evidence: `docs/SharedPrngConcurrencyPlan.md`
§11–§16.

**This did not touch SR-AUD-050 below.** The engine is still `std::mt19937_64`,
not the platform CSPRNG; the repair changed *who owns* the engine, not *what kind
of entropy it produces*, and per-thread ownership makes that finding neither
better nor worse. SR-AUD-050 stays `confirmed`.

### SR-AUD-050 — high — `NewGuid` uses a predictable standard PRNG instead of .NET's OS CSPRNG

The same lines seed `std::mt19937_64` once from `std::random_device` and then
derive every later UUID byte from that deterministic engine.  Neither
`std::mt19937_64` nor `std::random_device` has the .NET contract of obtaining
each UUID's random fields from the platform CSPRNG.  Current .NET documents
122 bits of strong entropy and uses the OS CSPRNG on non-Windows platforms;
its `CreateVersion7` source explicitly obtains its random fields through
`NewGuid`.  This repository already has a platform-backed
`RandomNumberGenerator::Fill` implementation (`getrandom` on Linux,
`BCryptGenRandom` on Windows, and `getentropy` on BSD/Darwin), which makes the
weaker local choice concrete rather than a platform limitation.

**Impact:** externally visible UUID output can be predicted from recovery of
the Mersenne Twister state, contrary to the reference's strong-entropy
behavior.  The public output is still a structurally valid v4/v7 UUID, but it
must not silently downgrade the reference randomness guarantee.  The v7 path
has the identical issue because it begins with `NewGuid`.

**Required post-audit verification:** introduce an explicitly platform-secure
byte-generation path available to Core without a dependency-cycle regression,
make its failure behavior deliberate and documented, and test it through a
deterministic injectable seam.  Add a bounded concurrent `NewGuid` and
`CreateVersion7` stress test and run it under TSan.  Shape/uniqueness samples
alone cannot prove either property.

### SR-AUD-043 (extended) — negative span lengths become unbounded strings

`Parse(ReadOnlySpan<char>)`, `Parse(ReadOnlySpan<bytecs>)`, and both
`TryParse` counterparts (lines 389–418) immediately cast `getLengthProperty()`
to `std::size_t` while building a `std::string`.  The existing Span finding
proves that public construction accepts a negative length.  Passing such a
malformed view here can therefore request a huge read/allocation before the
Guid grammar is consulted; it is not rejected like the 16-byte constructor,
which first compares its length to 16.

**Required post-audit verification:** repair the shared Span validity boundary,
then add negative-metadata assertions for char and UTF-8 `Parse`/`TryParse`
under ASan/UBSan.  Do not add a local cast workaround that leaves other Span
consumers unsafe.

## Other missing assertions and diagnostics

- There is no single concurrent invocation test for either UUID factory, and
  no TSan target exercises them.
- All `NewGuid` tests only inspect individual shape bits or a 20-value
  sequential uniqueness sample; neither exposes the race nor establishes the
  required secure entropy source.
- No parser test passes a deliberately malformed `ReadOnlySpan<char>` or UTF-8
  span length, despite these overloads doing raw signed-to-unsigned conversion.

## Final assessment

Formatting, byte order, and ordinary parsing have strong evidence.  UUID
generation has one sanitizer-confirmed concurrency defect (SR-AUD-010), a
separate high-severity secure-randomness parity defect (SR-AUD-050), and span
parsing extends the shared negative-length risk (SR-AUD-043).  No production
source was modified during this audit.
