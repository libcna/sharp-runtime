# Audit: `modules/threading/include/System/TimeProvider.hpp`

## Metadata

- AUDITED: 90-line public time-provider abstraction and inline operations,
  fully read.
- Validation: the complete `SharpRuntimeTests_Threading` executable passed
  359/359 on 2026-07-27; a standalone UBSan extreme-timestamp probe and C++/
  managed timer boundary probes were run.
- Related implementation evidence: `TimeProvider.cpp`, `Timer.hpp`, and
  `ITimer.hpp` are audited in this checkpoint.

## Extension of SR-AUD-131 — high — TimeProvider::GetElapsedTime repeats the signed timestamp subtraction overflow

Like the previously audited Stopwatch method, the inline implementation first
evaluates `endingTimestamp - startingTimestamp` in signed `longcs`.  The
fixed-frequency C++ probe calling `GetElapsedTime(INT64_MIN, INT64_MAX)`
reports UBSan signed integer overflow at the subtraction.  This is the same
root unsafe arithmetic pattern as SR-AUD-131 and extends that confirmed high
finding to the TimeProvider public API.

## Assessment

The system singleton, virtual time/zone/frequency hooks, normal local-time
clamp, and `CreateTimer` shape form a useful native abstraction.  The elapsed
arithmetic must nevertheless avoid undefined C++ behavior, and the delegated
timer input/disposal boundaries are covered by SR-AUD-190/SR-AUD-191.

## Other missing assertions and diagnostics

- Tests cover only equal, small positive, and one-frequency timestamp pairs;
  they omit SR-AUD-131 extrema, reversed/negative pairs, non-unit custom
  frequencies, zero/negative frequency diagnostics, precision, and cumulative
  overflow.
- `GetLocalNow` lacks min/max-with-offset, custom-zone/DST, nonzero fractional
  offset, saturation, and concurrent virtual-clock assertions.
- CreateTimer tests omit null callback, valid/invalid extreme TimeSpans,
  pause/rearm, period/reentrancy, post-disposal Change, callback lifetime, and
  any non-System TimeProvider implementation (SR-AUD-190/SR-AUD-191).

## Final assessment

SR-AUD-131 extends to this public API; no new independent high finding is
created.  No source or test was changed.

---

## Extension of SR-AUD-131 — REMEDIATED (ticket #2219, 2026-08-10)

The original evidence above is retained unchanged. **Only the SR-AUD-131 extension is closed**;
SR-AUD-190 and SR-AUD-191 in this report are untouched, and this ticket creates no new finding.

The full record is in the owning report
(`audit/modules/core/include/System/Diagnostics/Stopwatch.hpp.audit.md`) and in
`docs/CoreDefinedArithmeticBoundedParseFamilyPlan.md`. The one thing this report needs to carry is
the premise correction that belongs to **this** file: the extension is **two** undefined operations,
not one. `TimeProvider.hpp:74` had undefined behaviour at two columns — `:74:34`, the signed
subtraction this report describes, and `:74:55`, an out-of-range `double`→`long` conversion that
GCC's default `-fsanitize=undefined` set does **not** report because `float-cast-overflow` is
outside that group. The second is reachable **without** the first, on the **default** system
provider, through `GetElapsedTime(0, INT64_MAX)`, which returned the most negative representable
duration for a maximal positive interval.

Both are gone: the subtraction is now unsigned (CCF-004 class A, no value change) and the
conversion saturates (class C). All six `TimeProvider` probe cases exit 0 under
`-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all`; +11 permanent tests in
`TimeProviderTests.cpp`, including the `frequency <= 0` control proving `InvalidOperationException`
still wins before any arithmetic, a custom-frequency provider, and a `static_assert` that
`sizeof(TimeProvider)` and `alignof(TimeProvider)` are unchanged. **No member was added**, so no
layout or vtable change is possible. CCF-004 stays closed 8/8 and gains no member; audit numbering
stays frozen at 364.
