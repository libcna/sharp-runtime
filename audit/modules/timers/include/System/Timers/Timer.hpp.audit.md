# Audit: `modules/timers/include/System/Timers/Timer.hpp`

## Metadata

- AUDITED: public Timer state, lifecycle, documented reductions, and callback
  ownership contract.
- Validation: complete native Timers fixture passed 9/9. Direct native and
  current-.NET probes compared sender identity and a throwing Elapsed handler.

## SR-AUD-238 — high — an exception from Elapsed escapes the background std::thread and aborts the process

Timer exposes a handler collection whose callback runs directly on the
background Threading::Timer worker.  The native probe registers a one-shot
handler that throws `std::runtime_error`; the process prints
`terminate called after throwing` and exits 134.  Current .NET's
`Timer.MyTimerCallback` wraps its event invocation in `try/catch`, and the
same managed probe prints `throw_process=alive`.  A consumer callback can
therefore convert an ordinary notification failure into process termination.

## SR-AUD-239 — medium — Elapsed reports a null sender rather than the timer that raised the event

The public event uses an `Object* sender`, but the implementation raises it
with `nullptr`.  The native one-shot probe prints
`fired=1 sender_is_timer=0 sender_is_null=1`; current .NET prints
`fired=True sender_is_timer=True sender_is_null=False`.  The documented
Component reduction does not document replacing the event source identity with
null, so handlers cannot recover the timer instance through their supplied
sender.

## Assessment

The public lifecycle and interval shape are recognizable, and the header makes
the SynchronizingObject/Component reduction explicit.  Its documented raw
`this` callback lifetime caveat remains a serious design risk, but this audit
does not count it separately without a deterministic reproducer; SR-AUD-238
and SR-AUD-239 are independently demonstrated observable failures.

## Other missing assertions and diagnostics

- Add a process-isolated throwing-handler regression (SR-AUD-238), a
  non-null/same-instance sender assertion (SR-AUD-239), multiple handler and
  handler-removal coverage, and a handler that throws while another handler
  remains subscribed.
- Add synchronization/lifetime tests for Close, destructor, self-disposal,
  disable/re-enable, interval and AutoReset mutation from handlers, and a
  pending tick racing destruction; run the latter under ASan/TSan.
- Test fractional/maximum/non-finite intervals, TimeSpan overflow, elapsed
  SignalTime range, BeginInit repetition, and use-after-Dispose semantics.

## Final assessment

SR-AUD-238 and SR-AUD-239 are directly reproduced. No source or test was
changed during this audit.

---

## Post-audit remediation for SR-AUD-238 (ticket #2154, 2026-08-09): REMEDIATED

The audit evidence above is retained unchanged. **SR-AUD-239 is untouched and stays `confirmed`**
— its only repair is an object-layout and vtable change, held by the blocked ticket #2155. Cause
**TM-A** of `docs/SystemTimersNamespaceReviewPlan.md`; the full record is that plan's §14.

**Two extensions to the finding, both measured before any production change**
(`build-probe/2153_probe1_before.log`):

1. **The escape point is in another module.** The finding is filed against these files, but the raw
   `std::thread` entry point the exception actually escapes is `System::Threading::Timer::run`,
   which calls `s->callback(s->arg)` with no `try`/`catch` inside
   `std::thread([s = state_]() { run(s); })`. The repair still belongs here: .NET's
   `System.Threading.Timer` does **not** catch either — an unhandled callback exception on a
   thread-pool thread terminates the process there too — so the layer below is already correct and
   was left unchanged, with its behaviour pinned.
2. **The abort was universal and non-local.** 7 of 7 `SIGABRT`: `std::runtime_error`,
   `System::ArgumentException` and a non-`std` `throw 42`; one-shot and periodic; on the first fire
   and after three successful ones; and in one case an entirely **unrelated** second timer died with
   it, because the failure is process death rather than thread death.

**Repair.** One `try { … } catch (...) {}` around the **whole** callback body — not only
`Elapsed.Raise` — silent, matching `Timer.MyTimerCallback`, whose `throw_process=alive` this report
itself measured. `catch (...)` rather than `catch (const std::exception&)`, because `throw 42` is
one of the measured cases.

**A test weakness found by mutation testing, and fixed.** Narrowing the production catch to
`catch (const std::exception&)` **survived** the new suite as first written: the exception unwinds on
a background thread, and every throwing test stopped waiting the moment the handler *signalled*, so
the test finished before the abort landed. Each case now starts a **sentinel** periodic timer and,
after the throwing invocation, waits for it to fire five more times — which both gives the abort
time to happen and proves the process is alive when it has not. Swallowing *and* stopping the timer
gives one clean, precisely-targeted failure.

**Evidence.** +10 tests (`TimerExceptionBoundaryTests.cpp`); `SharpRuntimeTests_Timers`
**9 → 19**, every wait a condition variable rather than a sleep. ASan + UBSan + LSan and TSan over
three rounds of a two-handler timer throwing on every tick while the main thread reschedules:
**0 reports each**. No public signature, `noexcept`, virtual, vtable, data member or object-layout
change; component graph unchanged at 41 / 92.

**Pinned rather than changed:** a second subscribed handler is not reached when the first throws,
because the exception unwinds out of `EventHandler::Raise`'s loop — matching a C# multicast
delegate, where an exception in one target also stops the rest of the invocation list.

---

## Design record for SR-AUD-239 (ticket #2155, 2026-08-09): CONFIRMED (DESIGN-COMPLETE), BLOCKED

The audit evidence above is retained unchanged, and **SR-AUD-239 remains open**. Cause **TM-B** of
`docs/SystemTimersNamespaceReviewPlan.md` §4.4; the implementation is blocked on approval.

**`nullptr` is not a forgotten argument — it is the only value that compiles.**
`EventHandler<T>::Raise(System::Object* sender, const T& e)` types the sender as `Object*`.
`System::Timers::Timer` does not derive from `System::Object`:
`std::is_convertible_v<Timer*, Object*>` is **0**. `System::Object` is **abstract** (pure virtual
`GetTypeName()`) and **polymorphic**, so passing the timer requires giving `Timer` that base.

**Measured cost** (`build-probe/2153_probe4_layout.log`):

| Property | Now | With the `Object` base |
|---|---|---|
| `sizeof(System::Timers::Timer)` | **104** | **112** |
| `alignof` | 8 | 8 |
| `std::is_polymorphic_v` | false | **true** |
| every data member's offset | — | **shifted by 8** |
| new virtual members | — | `~Object`, `GetTypeName`, `ToString`, `Equals`, `GetHashCode` |

`GetTypeName()` must additionally be implemented, because `System::Object` is abstract.

**No compatible alternative exists.** There is no `void*` sender overload on `EventHandler`, the
sender is a parameter rather than a property of `ElapsedEventArgs`, and handing out some *other*
`Object*` would be worse than null.

An object-layout **and** vtable change on a public type needs explicit per-action user approval
under this repository's settled rules — #1788 and #1789 both required one for smaller growth. The
approval sentence is recorded verbatim in ticket #2155.

**Current behaviour is pinned** by a runtime test and by compile-time `static_assert`s in two test
files, so the change cannot land silently: the build stops until the pins are deliberately updated.
