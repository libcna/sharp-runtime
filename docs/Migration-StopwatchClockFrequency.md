<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Stopwatch::Frequency` is the clock's, not the `TimeSpan` tick rate (ticket #2326)

*2026-08-18.* `Stopwatch::Frequency` was `10'000'000` and `GetTimestamp()` divided the clock's
nanosecond count by 100. .NET reports the **platform's** frequency — 1,000,000,000 on Unix.

Landed under `docs/StandingApprovals.md` **SA-8**. **Timestamp values change scale by 100×** and
`GetElapsedTime`'s conversion is no longer the identity. No signature, layout, vtable or
`noexcept` change, and `Frequency` stays `constexpr`.

---

## 1. What changed

| | Was | Is |
|---|---|---|
| `Stopwatch::Frequency` | `10'000'000` | **`1'000'000'000`** |
| `GetTimestamp()` | nanoseconds **÷ 100** | the clock's raw nanosecond count |
| `GetElapsedTime(a, b)` scale factor | exactly `1.0` | **`0.01`** — .NET's `s_tickFrequency` |
| `GetElapsedTime(1000, 3000)` | `2000` ticks | **`20`** ticks (2000 ns *is* 20 ticks) |
| `TimeProvider::getTimestampFrequencyProperty()` | `10'000'000` | **`1'000'000'000`** (it forwards) |
| `Elapsed`, `ElapsedMilliseconds`, `ElapsedTicks` on an instance | — | **unchanged** |
| every measured duration | — | **unchanged, and finer** |

## 2. Why the old value was a defect, not merely a different choice

The old pair was **self-consistent**: `Frequency` was `TimeSpan::TicksPerSecond`, timestamps were
100-ns ticks, and every elapsed time it computed was correct. What it could not do was **resolve**
anything finer than 100 ns — `GetTimestamp()` threw the low two digits away, so two events 30 ns
apart received the *same* timestamp here and different ones in .NET.

A test asserts the resolution directly: two back-to-back `GetTimestamp()` calls must be able to
differ by less than 100.

## 3. The value is derived, not transcribed

```cpp
static constexpr longcs Frequency = Clock::period::den / Clock::period::num;
```

.NET's Windows build returns `QueryPerformanceFrequency`. This port samples
`std::chrono::steady_clock`, so **reporting QPC's frequency would be a lie about a different
timer**. Reporting the frequency of the clock actually read is the correct answer, and on both
libstdc++ and MSVC `steady_clock::period` is `std::nano` — .NET's own Unix answer.

It also stays `constexpr`, where .NET's is a runtime `static readonly`. That is a *stronger*
guarantee than .NET gives, not a weaker one.

## 4. To migrate

A stored timestamp from a previous build is no longer comparable with a new one — but that was
never a durable value anyway (`steady_clock`'s epoch is unspecified and process-local).

```cpp
// converting a timestamp delta to seconds: unchanged, and now more precise
double seconds = double(t2 - t1) / Stopwatch::Frequency;

// code that assumed timestamps were TimeSpan ticks
TimeSpan span = TimeSpan::FromTicks(t2 - t1);        // WRONG since #2326
TimeSpan span = Stopwatch::GetElapsedTime(t1, t2);   // right, and always was
```

Instance measurement — `Start`/`Stop`/`Elapsed`/`ElapsedTicks` — is **completely unaffected**; it
never went through `Frequency`.

## 5. What this ticket had to re-measure, and why

The review warned that `TimeProvider::GetElapsedTime`'s scaling factor was *"exactly 1.0 precisely
because `Frequency == TimeSpan::TicksPerSecond`"*, and that changing `Frequency` re-enables a
non-unit scale on a path carrying CCF-004 saturation work. That was correct, and twelve tests
across two suites had to be rescaled.

**None of them was a regression.** They were CCF-004's evidence that the subtraction is *defined*
rather than undefined, and that claim is untouched — the delta still wraps to exactly the
two's-complement value. What moved is the reported tick count, because a scale is now applied, and
every new expectation is **.NET's own**: `(long)((end - start) * 0.01)` with the saturating
float-to-integer conversion modern .NET adopted.

Two of them needed more than a number. `SignedSweepAtUnitFrequency` and
`PrecisionAbovePow2_53IsAPreExistingProperty` are about `double` rounding at unit scale, and the
system provider only *happened* to have unit frequency; they now construct a
`FixedFrequencyProvider(TicksPerSecond)`, which is what their names always claimed.

Two saturation pins stopped reaching the guard, because `0.01 × INT64_MAX` is comfortably
representable. The guard is still exercised — by `CustomFrequency_ScalingLeavingRangeSaturates`,
which supplies its own frequency and is unaffected.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `Stopwatch::Frequency`, `Stopwatch::GetTimestamp` or
`getTimestampFrequencyProperty` — **zero sites in both**. Neither repository was modified.
