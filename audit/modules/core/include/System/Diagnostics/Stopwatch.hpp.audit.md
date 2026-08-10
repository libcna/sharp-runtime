# Audit: `modules/core/include/System/Diagnostics/Stopwatch.hpp`

## Metadata

- AUDITED: 151-line inline stateful stopwatch, fully read.
- Validation: `StopwatchTests.*` passed 20/20 in `SharpRuntimeTests_Core_Base`
  on 2026-07-26.
- Reproduction: UBSan build of `/tmp/sharp-runtimervc-stopwatch-audit-probe`
  prints `frequency=10000000`, then reports signed overflow at
  `Stopwatch.hpp:133` for `INT64_MAX - INT64_MIN` and exits 1.
- Reference basis: local .NET `System/Diagnostics/Stopwatch.cs:9-109` and
  `Stopwatch.Unix.cs:5-14`.

## SR-AUD-130 — medium — Stopwatch publishes a fabricated 10 MHz timestamp frequency instead of the platform timer unit

On Unix current .NET exposes `Frequency = 1_000_000_000` and `GetTimestamp()`
returns the native monotonic nanosecond counter. The C++ port divides its
steady-clock nanoseconds by 100, hardcodes the observable frequency to
10,000,000, and documents the altered timestamps as the counterpart API. Its
internal elapsed TimeSpan happens to remain self-consistent, but callers that
observe `Frequency`, persist timestamps, compare with platform/.NET values, or
compute elapsed duration using the documented `timestamp / Frequency` contract
receive a different public unit. The direct fixture asserts the fabricated
10 MHz value.

Retain raw steady-clock units with a matching platform frequency and perform
TimeSpan conversion only at the public elapsed boundary, or explicitly make
this a differently named/documented C++ timestamp API rather than a .NET
Stopwatch counterpart.

## SR-AUD-131 — high — GetElapsedTime performs attacker-controlled signed timestamp subtraction with undefined overflow

`GetElapsedTime(startingTimestamp, endingTimestamp)` directly evaluates
`endingTimestamp - startingTimestamp` as signed `longcs` before constructing
TimeSpan. The public values `INT64_MIN` and `INT64_MAX` reach UBSan-confirmed
undefined behavior. Current .NET performs its arithmetic in the defined C#
unchecked-integral model before scaling timer ticks; native C++ must use a
defined emulation (for example unsigned modular subtraction plus the chosen
documented range policy) rather than rely on signed overflow.

The same unchecked addition pattern exists while accumulating a very long
running/stopped interval, but the static two-timestamp overload provides the
immediate public reproduction.

## Other missing assertions and diagnostics

- Tests omit raw timestamp-unit parity, elapsed calculation from externally
  supplied raw timestamps, and non-Unix/platform-frequency behavior.
- Missing extreme timestamp, reversed timestamp, accumulator-overflow, clock
  epoch, and monotonicity-under-wall-clock-adjustment vectors.
- No synchronization contract or concurrent Start/Stop/read behavior is
  documented; mutable state has no locking.
- Sleep-based lower-bound tests are timing-sensitive and do not assert a
  tolerable upper bound or scheduling diagnostic.

## Final assessment

Normal start/stop mechanics use a monotonic clock and pass their focused suite,
but public timestamp units and extreme timestamp arithmetic have confirmed
SR-AUD-130/131 gaps. No source or test was modified during this audit.

---

## SR-AUD-131 — REMEDIATED (tickets #2218 + #2219, 2026-08-10)

The original evidence above is retained unchanged. **Only SR-AUD-131 is closed by these two
tickets**; SR-AUD-130 (the fabricated 10 MHz public timestamp frequency) and the missing
synchronisation contract in this report stay `confirmed` and were deliberately not touched.

The finding needed **two** tickets because it spans two modules: `Stopwatch` (#2218) and
`TimeProvider` (#2219, whose report records the extension). It is `remediated` only now that both
have landed, following the rule `docs/DefinedArithmeticBoundaryPlan.md` §5 states for a multi-site
member. The family record is
`docs/CoreDefinedArithmeticBoundedParseFamilyPlan.md`.

**This is a CCF-004 *occurrence*, not a new CCF-004 member.** CCF-004 stays **closed 8/8** and is
neither edited nor reopened, exactly as the cross-cutting record already does for T-F, T-A, T-C and
N-B. No `SR-AUD-*` identifier was issued; numbering stays frozen at **364**.

**Three corrections to the finding's premises, every one measured** (probe
`build-probe/2217_probe_before.cpp`, `-O0`, `volatile` operands, one process per case).

1. **`TimeProvider::GetElapsedTime` carries a second, independent undefined operation that the
   finding does not name, and GCC's default `-fsanitize=undefined` set cannot see it.** The line
   has undefined behaviour at **two columns**: `:74:34` is the signed subtraction the finding
   describes, and `:74:55` is an out-of-range `double`→`long` conversion ([conv.fpint]/1) reported
   only with `-fsanitize=float-cast-overflow`, which is **not** in GCC's `undefined` group.
   `GetElapsedTime(0, INT64_MAX)` on the **default system provider** reaches the second **without**
   reaching the first — `INT64_MAX - 0` does not overflow — and returned
   `ticks = -9223372036854775808`, a maximal *negative* duration for a maximal *positive* interval.
   A repair aimed only at the subtraction would have left that silent wrong answer in place.
2. **Four public doors reach a defective site, not the one the finding names.** `Stopwatch`'s two
   `GetElapsedTime` overloads and `TimeProvider`'s two. `Stopwatch::GetElapsedTime(0, INT64_MAX)`
   is *not* defective while `TimeProvider`'s call with the same arguments *is*, because only the
   latter routes through a `double`.
3. **`TimeProvider::GetLocalNow` was inventoried and is clear** — its tick-plus-offset sum cannot
   leave `int64` given the two operand domains, and the clamp that follows already uses CCF-004's
   own unsigned-compare idiom. A count that does not move is recorded as deliberately as one that
   does.

**What changed.** Both subtractions are now performed in the unsigned domain and converted back
(CCF-004 class A): **every value is byte-identical**, measured on both sides — `(INT64_MIN,
INT64_MAX)` → `-1`, `(INT64_MAX, INT64_MIN)` → `1`, `(-1, INT64_MAX)` → `INT64_MIN`, `(1000, 3000)`
→ `2000`. `TimeProvider`'s conversion now **saturates** to `INT64_MAX`/`INT64_MIN` (and a NaN, which
is unreachable for a finite delta and a positive finite frequency, to `0`) instead of converting out
of range. That is CCF-004 class C and is the family's only value change here:
`GetElapsedTime(0, INT64_MAX)` returns `INT64_MAX` rather than `INT64_MIN`, and now agrees with
`Stopwatch::GetElapsedTime` on the same arguments. The compatible-narrowing argument is the one this
repository accepted for #1817, #1818, #1825, #1836 and #1837: **the corrected results were
undefined behaviour producing a wrong sign**, so no caller can have depended on them, and the
values are not guaranteed between two builds of this repository.

`Stopwatch::Stop()` and `currentNs()` carry the same additive shape on the private nanosecond
accumulator and were converted alongside. That site is **not publicly reachable** — roughly 292
years of measured interval, and `elapsed_ns_` has no setter — so it is recorded as hardening and no
probe case claims otherwise.

**One property pinned rather than changed.** `TimeProvider::GetElapsedTime` scales through a
`double`, exactly as .NET's own `TimeProvider.GetElapsedTime` does, so a tick count above 2^53 is
not represented exactly and `(double)(INT64_MIN + 1)` rounds to exactly −2^63. That precision loss
is pre-existing, is untouched by these tickets, and is now pinned by a test so a later reader does
not mistake it for saturation. `Stopwatch`'s own overload never routes through a `double` and stays
exact.

**Closure evidence.** UBSan **plus `float-cast-overflow`** at `-O0`, one process per case, linked
against instrumented header-only production bodies: all twelve `Stopwatch`/`TimeProvider` cases exit
**0** under `-fno-sanitize-recover=all`, with every before-diagnostic absent. Activation is proved in
the same binary by a site deliberately left unrepaired (`Linq.hpp:236`, exit 1). +9 tests in
`StopwatchTests.cpp` and +11 in `TimeProviderTests.cpp`; `StopwatchTests` 29/29,
`SharpRuntimeTests_Threading` `TimeProvider*` 21/21. **Six mutations**, of which two are killed only
by the sanitizer probe and are labelled as the weaker signal they are — a value-preserving class A
repair cannot be pinned by values.

**Source, ABI and layout consequences: none.** `Stopwatch` gains two private `static constexpr
noexcept` member functions (neither virtual nor a data member); `TimeProvider` gains **no member at
all** — its saturation is written inline in the existing body precisely so that
`sizeof`/`alignof`/vtable stay provably untouched, which a `static_assert` in the test now pins.
No signature, `noexcept` specification, default argument or mangled symbol changed.
