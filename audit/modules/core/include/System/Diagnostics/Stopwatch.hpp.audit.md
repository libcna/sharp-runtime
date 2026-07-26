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
