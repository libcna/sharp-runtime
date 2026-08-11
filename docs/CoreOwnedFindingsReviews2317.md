<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Five independently reviewed `modules/core` findings (#2317 ownership pool)

Reviewed 2026-08-11. **These five are not a family.** They were picked in the order #2317
records, they live in five different headers, and they have five different root causes. The
only property they share is that #2317 was holding their ownership because no other open
ticket did — which is a bookkeeping fact, not a cause. Nothing here mints or extends a CCF,
and no `SR-AUD-*` identifier is created; numbering stays frozen at 364. **All five stay
`confirmed`; nothing is implemented.**

Each finding has exactly one ticket, and that ticket *is* its review: its description carries
the measurement and its status carries the disposition. That is deliberate — a separate
`done` review ticket plus a forward ticket, as SR-AUD-174 and SR-AUD-105 got, would have
produced ten tickets for five review-only outcomes.

| Finding | Header | Root cause | Ticket | State |
|---|---|---|---|---|
| SR-AUD-068 | `ValueType.hpp` | a base class whose incompatible default is reachable, plus a reflection-shaped part that is a permanent deviation | #2322 | `needs_user` |
| SR-AUD-092 | `Exception.hpp` | a missing fallback diagnostic whose exact text is unverifiable | #2323 | `todo` (deferred) |
| SR-AUD-122 | `EventHandler.hpp` | a `const` in a public callback type | #2324 | `needs_user` |
| SR-AUD-123 | `ResolveEventHandler.hpp` | a return type with no "not resolved" state | #2325 | `needs_user` |
| SR-AUD-130 | `Diagnostics/Stopwatch.hpp` | a public constant that names a fabricated unit | #2326 | `needs_user` |

---

## SR-AUD-068 — `ValueType` (#2322, `needs_user`)

**Live state.** 51 lines; `Equals` is `this == &other`, `GetHashCode` is the object address
narrowed to `intcs`, `ToString` is the literal `"System.ValueType"`. The class is not
abstract and has an implicit public default constructor, so `System::ValueType v;` compiles.

**Measured consumer surface: nothing derives from it in production.** The only derived types
in the whole repository are `SimpleValueType` and `ConcreteValueType` in
`modules/core/tests/System/ValueTypeTests.cpp`. `System::Void` explicitly documents that it
does *not* derive from it. So the finding's harm — "a public `struct`-like C++ type that
derives from `ValueType` but does not reimplement all three members" — has **no in-repository
instance**; the exposure is entirely downstream.

**Why it is not autonomous.** It splits into a part that cannot be repaired and a part that
needs approval. The field-by-field `Equals`/`GetHashCode` and the runtime-type-name
`ToString` are reflection, which `CLAUDE.md` lists as a **permanent deviation, out of scope**
— they are not a TODO. What remains is making the incompatible default unreachable, and every
way of doing that is a **public source break** in a shipped header: a `protected` default
constructor (matching .NET's own `protected ValueType()`) breaks direct instantiation; making
the class abstract breaks it harder. That is the same class as SR-AUD-063, which is recorded
as "a public source break needing approval" and left `confirmed` for exactly this reason.

## SR-AUD-092 — default `Exception` message (#2323, `todo`, deferred)

**Live state.** `Exception::Exception() : message_("")` (`Exception.cpp:12-14`), so
`getMessageProperty()` and `what()` are both empty.

**Premise measurement the finding does not state.** Surveying every `*Exception.cpp` in
`modules/core/src/System` for a no-argument constructor: **18 subclasses supply a non-empty
default message and exactly one type does not — `System::Exception` itself.** (`ArgumentException()`
is "Value does not fall within the expected range.", and so on.) The blast radius of a repair
is therefore far narrower than "every exception": it is the base type constructed directly.

**Why it is deferred, not implemented.** Two independent blockers, and the first is the one
the ticket-triage guidance predicted.

1. **Exact reference text.** .NET's fallback is the `Exception_WasThrown` resource, a
   *formatted* string that interpolates the runtime type name. With `/rv` absent, its exact
   wording cannot be read. Inventing it produces a divergence no later test could tell from
   the real thing — the same class as **#2252/#2260**, and the reason those are deferred
   rather than guessed.
2. **The format argument does not exist here.** The message is
   "…type '{0}'…" where `{0}` is the runtime type name, and this port has no reflection
   (permanent deviation), so a derived exception cannot report its own name. Emitting a
   hard-coded `"System.Exception"` for every type would be *worse* than empty for any
   subclass that reached the base fallback — though, per the survey above, no shipped
   subclass currently does.

**Structurally decidable part, deliberately not split off:** the finding's own remark that
"reflection absence … does not require a completely blank base message" is true, but choosing
*what* to put there is choosing text, which is blocker 1. There is no behaviour-only
sub-clause to land ahead of it. Two tests pin the current empty message
(`ExceptionTests.DefaultCtorEmptyMessage`, `ExceptionNewTests.DefaultCtor_MessageEmpty`) and
would be retired by a repair.

## SR-AUD-122 — `EventHandler<TEventArgs>` takes `const TEventArgs&` (#2324, `needs_user`)

**Live state.** `using HandlerType = std::function<void(Object* sender, const TEventArgs& e)>`
(`EventHandler.hpp:88`).

**Why it is a decision, not a fix.** Dropping the `const` is asymmetric, and both directions
matter. Existing subscribers that take `const TEventArgs&` keep compiling, because a
`T&` argument binds to a `const T&` parameter. What breaks is the **raise** side: every
caller that passes a `const` event-args object to `Raise()`/`Invoke()` stops compiling, and
the event-args object must become non-`const` at the raiser. That is a public source break in
a template used by shipped types — `System::Timers::Timer::Elapsed` is
`EventHandler<ElapsedEventArgs>`, and `ObservableCollection`/`ReadOnlyObservableCollection`
and `XObject` carry their own handler aliases in the same shape. The .NET behaviour the
finding names (a subscriber mutating event data for acknowledgement/cancellation patterns) is
real, but enabling it changes the const-correctness contract of a published API.

## SR-AUD-123 — `ResolveEventHandler` cannot say "not resolved" (#2325, `needs_user`)

**Live state.** `using ResolveEventHandler = std::function<std::string(void*, ResolveEventArgs&)>`
— a total function into `std::string`, with empty already meaning "absent requesting
assembly" elsewhere in `ResolveEventArgs`, so empty cannot also mean "unresolved".

**Measured consumer surface: none.** Outside its own header the alias appears only in
`modules/core/tests/System/Batch4Tests.cpp` (two tests: a handler returning a name, and a
null handler being falsy). It is **not wired to any `AppDomain` resolve API**, because those
are stubs under SR-AUD-103.

**Why it is a decision.** Adding the missing state is a **public representation change** to a
published alias: `std::optional<std::string>` (breaks every handler's return statement and
every consumer's use), or a documented sentinel (keeps the signature but cannot be enforced
by the type, which is the defect), or leaving it and documenting the reduction. Which one is
right depends on a question this repository has not answered: whether the assembly-resolution
surface is ever going to be more than a stub. Sequencing it ahead of SR-AUD-103 would fix the
signature of a delegate nothing calls.

## SR-AUD-130 — `Stopwatch::Frequency` is a fabricated 10 MHz (#2326, `needs_user`)

**Live state.** `static constexpr longcs Frequency = 10'000'000LL` (`Stopwatch.hpp:111`), and
`GetTimestamp()` returns steady-clock nanoseconds divided by 100. The header documents the
value as matching ".NET tick resolution", and the fixture asserts it.

**Premise addition the finding does not state: there is a second public surface.**
`System::TimeProvider::getTimestampFrequencyProperty()` returns `Stopwatch::Frequency`
verbatim and `TimeProvider::GetTimestamp()` forwards to `Stopwatch::GetTimestamp()`
(`modules/threading/include/System/TimeProvider.hpp:60-67`). Any change to the unit changes
`TimeProvider` too — including `TimeProvider::GetElapsedTime`, whose body already carries the
CCF-004/SR-AUD-131 saturation work and whose scaling factor is currently exactly 1.0 *because*
`Frequency == TimeSpan::TicksPerSecond`. A repair that changes `Frequency` re-enables a
non-unit scale factor on that path, so it must be re-measured there, not only in `Stopwatch`.

**Why it is a decision.** The finding itself offers two mutually exclusive remedies —
publish the platform unit (1 ns on Unix, the QPC frequency on Windows) and convert only at
the elapsed boundary, or keep the current unit and stop presenting it as a `Stopwatch`
counterpart. The first changes a **public `constexpr` constant** and the meaning of every
timestamp a caller has stored or compared; the second is a documentation/naming change that
concedes the divergence. Both are legitimate; picking one is not this review's call.
