<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Timers` namespace review and remediation plan

Ticket **#2153**, written 2026-08-09 on branch
`claude/remediation-batch-1804-namespace-b1yjh5`.

The eighteenth namespace review in the post-audit remediation programme, after `System::Threading`
(#1950), `Threading::Tasks`/`Channels` (#1964), `System::Runtime` (#1972), `System::Uri` (#1987),
`System::Text` (#2006), `System::Diagnostics` (#2023), `System::Net` (#2034), `Buffers` (#2054),
`Net::Http` (#2062), `System::Xml` (#2074), `Net::WebSockets` (#2087), `System::IO` (#2098),
`Text::Json` (#2109), `Net::Http::Headers` (#2124), `Net::Sockets` (#2133), `IO::Hashing` (#2140)
and `IO::Compression` (#2147).

Same contract as its seventeen predecessors: **every confirmed finding in the module gets exactly
one disposition, no finding disappears between the audit index and this plan, and every premise is
re-measured against the shipped library before it is relied upon.**

**Nothing in §§1–11 is implemented by writing them.** The measured before-matrices are
`build-probe/2153_probe1_before.log` (behaviour), `build-probe/2153_probe2_fco.log` (the
undefined-conversion half), `build-probe/2153_probe3_before.log` (the interval domain) and
`build-probe/2153_probe4_layout.log` (the ABI measurement for SR-AUD-239).

**No `SR-AUD-*` identifier is issued by this review.** Audit numbering stays frozen at **364**.

---

## 1. Why `modules/timers` — the selection, re-derived and compared

Re-derived by parsing `audit/AUDIT_FINDINGS_INDEX.md` on 2026-08-09, **not** inherited from the
previous handoff, which recommended this module. Every **unreviewed** module with ≥ 2 open
confirmed findings:

| Module | Open | high | med | `/rv`-bound? | Verdict |
|---|---|---|---|---|---|
| `core` | 72 | 9 | 59 | mixed | **not a namespace** — already carved into seven `CCF-*` family plans; excluded by seventeen precedents |
| `globalization` | 7 | 1 | 6 | **heavily** | needs `/rv` **and** ICU data, neither present; its high is a process-global-state race needing approval |
| `time-zone` | 7 | 0 | 7 | **mostly** | zero high; 3 of 7 need a real tz database plus .NET parity |
| `numerics` | 4 | 0 | 4 | partly | zero high; one finding is a missing-definition link error |
| `xml-linq` | 4 | 1 | 3 | no | its **only** high is CCF-019 — **blocked** (#1899) |
| `net-network-information` | 3 | 0 | 3 | no | zero high; its executable cannot pass here (#1962 capability gap) |
| **`timers`** | **2** | **1** | **1** | **none** | **winner** |
| `console` | 2 | 0 | 2 | no | zero high; two argument-domain items |
| `security-cryptography` | 2 | **2** | 0 | no | the only real rival — see §1.1 |

### 1.1 Why a two-finding unit is worth reviewing at all, and why it beats the 100 %-high rival

The programme's earlier reviews used a ≥ 6-finding threshold. This unit is below it, and the
justification is recorded rather than assumed:

1. **The severity's character is the highest available.** SR-AUD-238 is not a wrong answer, a
   leak, or a divergence — it **terminates the process**. Measured: an ordinary `Elapsed` handler
   that throws kills the program with `SIGABRT`, in **7 of 7** cases, including one where a second,
   entirely unrelated timer was running and died with it. No other unreviewed finding converts a
   consumer-code mistake into process death.
2. **Zero reference dependence.** Both findings are locally decidable, and both already carry a
   **managed probe result recorded in the audit itself** — `throw_process=alive` and
   `sender_is_timer=True` — so the .NET answer is measured, not recollected. `/rv` being absent
   costs this unit nothing.
3. **Coherent and completable.** One namespace, 4 headers, 1 body, 1 test file, **279 lines** of
   production code. The whole module can be dispositioned in one pass.
4. **It closes a module.** After this review `modules/timers` has no undispositioned finding left.

**Against `security-cryptography` (2 findings, both high, 100 %):** its two findings are real and
in scope — `CLAUDE.md` excludes symmetric/asymmetric crypto but keeps `HMAC`/PBKDF2 explicitly in
scope — but they lose on two measurable grounds. Their consequence class is *residual key material
after `Dispose`*, which is only exploitable with a separate memory-disclosure primitive, whereas
SR-AUD-238 is unconditional process death from ordinary use. And SR-AUD-331 needs a disposed-state
flag on `Rfc2898DeriveBytes`, i.e. an **object-layout change**, so that module could not be closed
in one pass either. `timers` is chosen because its worst finding is worse *and* its compatible
fraction is higher.

---

## 2. Scope and file inventory

Component **`Timers`** → target `sharp_runtime_timers`,
`PUBLIC_DEPENDENCIES ComponentModel Core.Base`, `PRIVATE_DEPENDENCIES Threading`.

| File | Lines | Role |
|---|---|---|
| `include/System/Timers/Timer.hpp` | 105 | the whole public type |
| `include/System/Timers/ElapsedEventArgs.hpp` | 25 | `signalTime` carrier |
| `include/System/Timers/ElapsedEventHandler.hpp` | 18 | a `std::function` alias |
| `include/System/Timers/TimersDescriptionAttribute.hpp` | 25 | attribute stub |
| `src/System/Timers/Timer.cpp` | 106 | the whole implementation |
| `tests/System/Timers/TimerTests.cpp` | 93 | **9 tests**, the module's entire coverage |

### 2.1 Public surface inventory

| Member | Notes |
|---|---|
| `Timer()`, `Timer(double)`, `Timer(TimeSpan)` | interval validation lives here — see §4.3 |
| `~Timer()` | calls `Close()` |
| copy ctor / copy assignment | `= delete` |
| `getAutoResetProperty` / `setAutoResetProperty` | |
| `getEnabledProperty` / `setEnabledProperty` | the start/stop door |
| `getIntervalProperty` / `setIntervalProperty` | **validates less than the constructor** — §4.3 |
| `Start()` / `Stop()` | thin wrappers over `setEnabledProperty` |
| `BeginInit()` / `EndInit()` | delayed-enable batch |
| `Close()` / `Dispose()` | `Dispose()` is `Close()` |
| `Elapsed` | a public `System::EventHandler<ElapsedEventArgs>` **field**, not an accessor pair |

Deliberate reductions already documented in the header and **not** re-litigated here: no
`Component` base, no `SynchronizingObject`, and the raw-`this`-capture lifetime caveat (the same
class of hazard as `Socket`'s async members and `ClientWebSocket`'s Send/Receive, tracked in those
namespaces' own blocked design tickets).

---

## 3. Confirmed finding inventory — both, with measured current behaviour

| ID | Sev | Owner | Defect | Cause |
|---|---|---|---|---|
| **SR-AUD-238** | **high** | `Timer.cpp`'s callback lambda | an exception from an `Elapsed` handler escapes the background `std::thread` entry point and calls `std::terminate` | **TM-A** |
| **SR-AUD-239** | med | `Timer.cpp:62`, `Timer.hpp` | `Elapsed.Raise(nullptr, args)` — handlers get a null sender instead of the raising timer | **TM-B** |

Both reproduce exactly as the audit describes. The corrections below **extend** them; neither is
overturned.

---

## 4. Corrections and extensions to the audit record — measured, not inferred

### 4.1 The escape point is in `modules/threading`, not `modules/timers`

The finding is filed against `Timer.hpp`/`Timer.cpp`/`TimerTests.cpp` in **this** module. The raw
`std::thread` entry point the exception actually escapes is
`System::Threading::Timer::run` (`modules/threading/include/System/Threading/Timer.hpp`), which
calls `s->callback(s->arg)` with no `try`/`catch`, inside a thread created as
`std::thread([s = state_]() { run(s); })`.

That matters for **where the repair goes**, and the answer is still *this* module:

- .NET's `System.Timers.Timer.MyTimerCallback` wraps its event invocation in `try`/`catch`; the
  audit's own managed probe measured `throw_process=alive`.
- .NET's `System.Threading.Timer` does **not** catch — an unhandled callback exception on a
  thread-pool thread terminates the process, which is what this port already does.

So the layer below is **already correct** and must not be changed; the missing `try`/`catch` belongs
in `System::Timers::Timer`'s own callback lambda. Measured confirmation that the two layers are
genuinely separate: a bare `System::Threading::Timer` whose callback throws also aborts here
(probe 1, last row of the SR-AUD-238 block), and that row is **pinned rather than repaired**.

### 4.2 The abort is universal across exception type, mode and fire count — and it is not local

The finding's probe is one throwing handler. Measured across the matrix:

| Case | Result |
|---|---|
| `std::runtime_error`, one-shot, first fire | **SIGABRT** |
| `std::runtime_error`, periodic, first fire | **SIGABRT** |
| `std::runtime_error`, periodic, after 3 successful fires | **SIGABRT** |
| `System::ArgumentException` (a sharp-runtime type), one-shot | **SIGABRT** |
| a **non-`std`** type (`throw 42`), one-shot | **SIGABRT** |
| a **second, unrelated** timer is running when the first throws | **SIGABRT** — it dies too |
| `System::Threading::Timer` callback throws directly | **SIGABRT** — the layer below, §4.1 |

7 of 7. **One timer's handler kills every timer**, because the failure is process death, not thread
death. That is the sentence the finding does not contain and the reason this is the unit's high.

### 4.3 A third defect the audit does not record: the interval domain, and undefined behaviour

Not a new `SR-AUD-*` identifier — a post-audit defect, in the sense the previous namespace batches
used. **The setter validates strictly less than the constructor**, and every value in the gap
reaches an undefined float-to-integer conversion:

| Value | `Timer(double)` | `setIntervalProperty` | then `Start()` |
|---|---|---|---|
| `0`, `-1`, `-inf` | `ArgumentException` | `ArgumentException` | — |
| `0.5`, `1`, `2147483647` | accepted | accepted | works |
| **`NaN`** | **accepted** | **accepted** | `ArgumentOutOfRangeException("dueTime")` |
| **`+inf`** | `ArgumentException` | **accepted** | `ArgumentOutOfRangeException("dueTime")` |
| **`2147483648`** | `ArgumentException` | **accepted** | `ArgumentOutOfRangeException("dueTime")` |
| **`3e9`** | `ArgumentException` | **accepted** | `ArgumentOutOfRangeException("dueTime")` |

Three consequences, all measured:

1. **`NaN` is accepted by both doors.** `interval <= 0` and `std::ceil(interval) > kMaxInterval`
   are both false for `NaN`.
2. **The conversion is undefined behaviour.** `static_cast<SharpRuntime::intcs>(std::ceil(interval_))`
   at `Timer.cpp:51` is a floating-to-integral conversion of a value not representable in the
   destination — undefined per `[conv.fpint]/1`. UBSan says so:
   `Timer.cpp:51:56: runtime error: nan is outside the range of representable values of type 'int'`.
   **Note for every past and future "UBSan clean" claim in this repository: GCC's
   `-fsanitize=undefined` does NOT include `float-cast-overflow`**; it has to be asked for by name,
   which is why this survived.
3. **The diagnostic is wrong and comes from the wrong door.** The four accepted-then-rejected values
   surface as `ArgumentOutOfRangeException` naming **`dueTime`** — an internal parameter of the
   private `System::Threading::Timer` dependency — thrown from `Start()`, long after the door that
   is supposed to validate accepted the value.

### 4.4 SR-AUD-239's only repair is an object-layout change — measured

`EventHandler<T>::Raise(System::Object* sender, const T& e)` types the sender as `Object*`.
`System::Timers::Timer` does not derive from `System::Object`, so `Elapsed.Raise(this, args)` does
not compile: `std::is_convertible_v<Timer*, Object*>` is **0**. `System::Object` is **abstract**
(pure virtual `GetTypeName()`) and **polymorphic**. Giving `Timer` that base is the only way to pass
itself, and it costs:

| Property | Now | With the `Object` base |
|---|---|---|
| `sizeof(Timer)` | **104** | **112** |
| `alignof` | 8 | 8 |
| `std::is_polymorphic_v` | **false** | **true** |
| every data member's offset | — | **shifted by 8** |
| new virtual members | — | `~Object()`, `GetTypeName()`, `ToString()`, `Equals()`, `GetHashCode()` |

That is an object-layout **and** vtable change on a public type. Under this repository's settled
rules (#1788, #1789) it needs explicit per-action user approval, so SR-AUD-239 is **blocked** with a
completed design. No compatible alternative exists: there is no `void*` sender overload, the sender
is a parameter rather than a property of `ElapsedEventArgs`, and handing out some *other* `Object*`
would be worse than null.

---

## 5. Root causes

### TM-A — a consumer callback runs on a raw thread entry point with no exception boundary (SR-AUD-238)

`System::Timers::Timer::startTimerThread`'s lambda calls `Elapsed.Raise(...)` directly. `Raise`
invokes each subscribed `std::function` with no `try`/`catch`; the lambda is the
`System::Threading::Timer` callback; that callback is invoked from `run()`, which is the body of a
`std::thread`. An exception therefore leaves a thread's entry function, which is `std::terminate`
by definition. The exception boundary that .NET's `MyTimerCallback` provides has no counterpart
here.

### TM-B — the event's sender type requires a base class the type does not have (SR-AUD-239)

Not a forgotten argument: `nullptr` is the only value that *compiles*. §4.4.

### TM-C — two doors onto one field validate two different domains, and the narrower one is UB (post-audit)

`Timer(double)` and `setIntervalProperty` both write `interval_`, and only the constructor applies
the upper bound. Neither excludes `NaN`. §4.3.

---

## 6. Compatible / blocked / deferred matrix

| Cause | Ticket | Classification | Approval |
|---|---|---|---|
| TM-A | **#2154** | **compatible — implement now** | none |
| TM-B | **#2155** | **blocked — object layout + vtable change** | **required** |
| TM-C | **#2156** | **compatible — implement now** | none |
| disclosure + pins | **#2157** | compatible, mandatory closing ticket | none |

---

## 7. Source / ABI / layout / `noexcept` consequences

| Ticket | Signature | vtable | layout | `noexcept` | Accepted input | Observable result |
|---|---|---|---|---|---|---|
| #2154 | — | — | — | — | — | a throwing handler no longer kills the process |
| #2155 | — | **new virtuals** | **104 → 112** | — | — | `sender` becomes the timer |
| #2156 | — | — | — | — | `NaN`, `+inf`, `> 2^31` rejected by the setter | `ArgumentException` at the door instead of UB |

---

## 8. Callback, lifetime and concurrency consequences

- **#2154 changes when a handler's exception is observed, not whether the timer keeps running.**
  After it, a periodic timer whose handler throws continues to fire — which is .NET's behaviour and
  is the point of the repair, but it means a handler that throws every time now loops silently. The
  swallow is what .NET does (`throw_process=alive` in the audit's managed probe) and is pinned by a
  test that asserts subsequent fires still happen.
- **The `this`-capture hazard is out of scope and unchanged.** The header already documents it; it
  is the same shape as `Socket`'s and `ClientWebSocket`'s, both of which have their own blocked
  design tickets. Nothing in this review makes it better or worse.
- **`Close()` from inside a handler, rescheduling from inside a handler, double `Close()` and
  destruction while enabled all already survive** (probe 1's control block), and every ticket here
  must keep them surviving.

---

## 9. Test matrix

| Cause | Required coverage |
|---|---|
| TM-A | every exception kind (`std::`, sharp-runtime, non-`std`) × one-shot/periodic × first-fire/after-N; a second timer alive; the timer keeps firing afterwards; a second subscribed handler still runs; `Close()` after a failed callback; **process-isolated**, since the pre-repair behaviour is an abort |
| TM-B | pinned only: `sender == nullptr`, plus `std::is_convertible_v<Timer*, Object*> == false` as the compile-time pin that #2155 cannot land silently |
| TM-C | the full 10-value × 3-door domain table of §4.3, both constructors and the setter, plus the accepted values that must keep working (`0.5`, `1`, `2147483647`) |
| controls | the module's existing 9 tests; `Start`/`Stop`; `BeginInit`/`EndInit`; `AutoReset` both ways; reschedule and self-close from a handler |

**Deterministic synchronisation is mandatory**: every wait is a condition variable signalled by the
handler, with a timeout only as a failure backstop. No test may assert on a sleep.

## 10. Sanitizer matrix

| Cause | ASan | UBSan | **`float-cast-overflow`** | LSan | TSan |
|---|---|---|---|---|---|
| TM-A | required — the unwind path | required | — | required — a swallowed exception must not leak the timer state | **required** — the repair runs on the worker thread |
| TM-C | — | required | **required, by name** — §4.3's UB is invisible to GCC's `undefined` group | — | — |

---

## 11. Execution order

**#2154 → #2156 → #2157**, with #2155 blocked throughout. #2154 first: it is the only finding whose
current behaviour is process death.

---

## 12. Exclusions

1. **`System::Threading::Timer`'s own lack of a callback exception boundary** — measured (§4.1),
   matches .NET's `System.Threading.Timer`, pinned, not changed.
2. **The raw-`this` capture lifetime hazard** — documented in the header, same class as `Socket`'s
   and `ClientWebSocket`'s, out of scope for this review.
3. **`Component`/`SynchronizingObject`** — documented reductions, not findings.
4. **`TimersDescriptionAttribute`** — an attribute stub; attributes are a permanent deviation.
5. **#2155's `Object` base** — approval-gated.

---

## 13. Completion criteria

1. Both findings `remediated`, or `confirmed` with a completed design and a named blocked ticket.
2. No `Elapsed` handler exception can terminate the process.
3. No public interval door can reach an undefined float-to-integer conversion.
4. `SharpRuntimeTests_Timers` grows monotonically, with every new timing assertion driven by a
   condition variable rather than a sleep.
5. #2155's absence is pinned so it cannot land silently.

---

## 14. Implementation record — #2154 (cause TM-A, SR-AUD-238)

**Repair.** One `try { … } catch (...) {}` around the **whole body** of
`System::Timers::Timer::startTimerThread`'s callback lambda — not only around `Elapsed.Raise`,
because nothing on that path may reach the thread entry point, including a failure inside
`DateTime::getNowProperty` or `ElapsedEventArgs`. The catch is silent, matching .NET's
`Timer.MyTimerCallback`, whose behaviour the audit's own managed probe measured as
`throw_process=alive`.

`catch (...)`, not `catch (const std::exception&)`: `throw 42` from a handler is a measured case,
and a narrowed catch lets it through. That distinction is now a test, and it is the reason
mutation D matters (below).

**`System::Threading::Timer::run` is deliberately unchanged.** It is the raw thread entry point the
exception actually escapes (§4.1), but .NET's `System.Threading.Timer` does not catch either — an
unhandled callback exception on a thread-pool thread terminates the process there too. Changing it
would make this port diverge from .NET in the other direction, on a type in another module and
another namespace.

**Before / after.**

| Case | Before | After |
|---|---|---|
| `std::runtime_error`, one-shot | **SIGABRT** | survives |
| `std::runtime_error`, periodic, first fire | **SIGABRT** | survives, keeps firing |
| `std::runtime_error`, periodic, after 3 fires | **SIGABRT** | survives, keeps firing |
| `System::ArgumentException`, one-shot | **SIGABRT** | survives |
| non-`std` (`throw 42`), one-shot | **SIGABRT** | survives |
| an unrelated second timer is running | **SIGABRT — it died too** | it keeps firing |
| `System::Threading::Timer` callback throws | **SIGABRT** | **SIGABRT — unchanged, by design** |

**Mutation testing — and a test weakness it found.**

| # | Mutation | Result | Counts? |
|---|---|---|---|
| A | delete the boundary entirely | the test executable **aborts mid-run** | **No** — abort-only, which this batch's rules exclude. It does confirm the defect is real |
| C | swallow *and* stop raising (`cookie_ = nullptr` in the catch) | **1 clean failure** — `APeriodicTimerKeepsFiringAfterItsHandlerThrows` alone; the other 18 green | **Yes** |
| D | narrow the catch to `catch (const std::exception&)` | **SURVIVED the file as first written.** The exception unwinds on a background thread, and every throwing test stopped waiting the moment the handler *signalled* — so the test finished before the abort landed and passed against a broken boundary | **Detected only after the test was fixed** |

Mutation D is the one that earned its keep. The repair for it is structural: every throwing case now
starts a **sentinel** periodic timer first and, after the throwing invocation, waits for the
sentinel to fire five more times. That both gives the abort time to happen and proves the process is
still running when it has not. With the sentinel in place, mutation D kills the executable
(`terminate called after throwing an instance of 'int'`) — abort-only, so still not a *clean*
discrimination, but no longer a silent pass.

**Sanitizers.** ASan + UBSan + LSan and TSan over three rounds of a two-handler timer that throws on
every tick while the main thread reschedules and flips `AutoReset`: **0 reports each**, with the
`Timer.cpp` body compiled from source and instrumented. TSan initially reported a race on
`pthread_cond_destroy` — a **probe artefact**: `System::Threading::Timer::Dispose` detaches its
worker rather than joining it, so a detached thread outlives a latch with static storage duration.
The probe's latch is now deliberately leaked; nothing about the production path was changed to make
the report go away, and the artefact is recorded rather than deleted.

**Consequences.** No public signature, `noexcept`, virtual, vtable, data member or object-layout
change; the component graph is unchanged at **41 / 92**. `SharpRuntimeTests_Timers` **9 → 19**
(+10). Two behaviours are now documented on `Timer::Elapsed` because a subscriber cannot guess
them: a handler failure is **invisible**, and the timer **keeps running**.

One further behaviour was measured and **pinned rather than changed**: a second subscribed handler
is *not* reached when the first throws, because the exception unwinds out of
`EventHandler::Raise`'s loop. That matches a C# multicast delegate, where an exception in one target
also stops the rest of the invocation list.

**SR-AUD-238 is `remediated`.**

---

## 15. Implementation record — #2156 (cause TM-C, post-audit defect)

**Repair.** One file-local `validateInterval(value, paramName)`, applied by `Timer(double)` and
`setIntervalProperty` — and therefore by `Timer(TimeSpan)`, which delegates. It adds
`std::isnan(value)` to the constructor's existing `(0, INT32_MAX]` test, and gives the setter that
whole test instead of its `value <= 0` fragment. Each door keeps its own `paramName`
(`"interval"` / `"value"`) and the message shape is unchanged, so no diagnostic a caller already
saw changes.

**Before / after — the 10-value × 3-door table.**

| Value | ctor before | setter before | then `Start()` before | ctor after | setter after |
|---|---|---|---|---|---|
| `0`, `-1`, `-inf` | reject | reject | — | reject | reject |
| `0.5`, `1`, `2147483647` | accept | accept | works | accept | accept |
| **`NaN`** | **accept** | **accept** | `AOORE("dueTime")` | **reject** | **reject** |
| **`+inf`** | reject | **accept** | `AOORE("dueTime")` | reject | **reject** |
| **`2147483648`** | reject | **accept** | `AOORE("dueTime")` | reject | **reject** |
| **`3e9`** | reject | **accept** | `AOORE("dueTime")` | reject | **reject** |

`float-cast-overflow` over the whole domain matrix: **1 report before, 0 after.**

**Mutation testing — both count.**

| # | Mutation | Result | Counts? |
|---|---|---|---|
| E | drop the `std::isnan` test | **3 clean failures** — both door tables and the "a rejected write leaves the interval intact" pin | **Yes** |
| F | restore the setter's old `value <= 0` check | **2 clean failures** — the setter table and the same pin; the constructor table stays green, which is exactly the asymmetry the defect was | **Yes** |

**Consequences.** +8 tests; `SharpRuntimeTests_Timers` **19 → 27**. No public signature, `noexcept`,
virtual, vtable, data member or object-layout change; graph unchanged at **41 / 92**.

**Narrowing, deliberate and disclosed.** Four values that used to be accepted by the setter now
throw. Three of them (`+inf`, `2147483648`, `3e9`) were already rejected by the constructor, so the
change makes one type self-consistent rather than inventing a rule. The fourth, `NaN`, is a genuine
narrowing at **both** doors: .NET's own setter is `if (value <= 0) throw`, which a `NaN` also
passes, so .NET may accept it and convert it to a defined value. `/rv` is absent, so that could not
be confirmed — and it does not change the answer, because the current behaviour here is undefined
rather than merely different.

**Pinned, not changed.** The constructor stores `std::ceil(interval)` while the setter stores the
value verbatim, so `Timer(0.5)` reports an interval of `1` and a timer *set* to `0.5` reports `0.5`,
even though both schedule the same 1 ms tick. No finding names it and either answer is a public
observable change, so it is recorded in the header and pinned by a test.
