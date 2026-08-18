<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `AggregateException` composes its message unconditionally (ticket #2309)

*2026-08-18.* `AggregateException("outer", {a, b}).what()` is now `"outer (a) (b)"`. It used to be
`"outer"` — the contained diagnostics were **discarded** whenever the caller supplied a message.

`sizeof(AggregateException)` grows **192 → 224** and `sizeof(UnobservedTaskExceptionEventArgs)`
**208 → 240**. **Downstream consumers must be recompiled**; no source change is needed.

Landed under `docs/StandingApprovals.md` **SA-3** (a private data member on a public type, with no
vtable, mangled-symbol, signature or `noexcept` change, the before/after `sizeof` pinned by a
layout test, and the full gate).

---

## 1. What changed

| Expression | Was | Is |
|---|---|---|
| `AggregateException("outer", {a, b})` | `"outer"` | `"outer (a) (b)"` |
| `AggregateException("outer", a)` | `"outer"` | `"outer (a)"` |
| `AggregateException({a, b})` | `"One or more errors occurred. (a) (b)"` | **unchanged** |
| `AggregateException("bare")` | `"bare"` | **unchanged** — no leaves, no composition |
| `outer.Flatten()` | `"One or more errors occurred. (a) (b)"` | `"outer (a) (b)"` — the caller's message survives |
| `outer.Flatten().Flatten()` | — | `"outer (a) (b)"` — **does not accrete** |
| `Handle` rethrow | — | composed twice, deliberately — §4 |

## 2. Why the ticket was blocked, and what unblocked it

#2309 was blocked on two independent grounds.

**Ground 1 — representation.** Composition (C2) and preservation (C3/C4) cannot both hold under
one stored string. The ticket measured all five candidate models: closing C2 alone works, closing
C3/C4 alone works, and closing **both** yields `"custom outer (a) (b) (a) (b)"` and **grows
without bound under repeated `Flatten()`**. The only escape is a second `std::string`.

That is exactly SA-3's case, so the field is taken.

**Ground 2 — reference text.** The composed grammar, and whether `Flatten`/`Handle` pass the raw
or the composed message, were unverifiable. `/rv` settles both, and the answer is what makes the
second field work.

## 3. The grammar

```csharp
if (_innerExceptions.Length == 0) return base.Message;
sb.Append(base.Message); sb.Append(' ');
for (…) { sb.Append('('); sb.Append(_innerExceptions[i].Message); sb.Append(") "); }
sb.Length--;                                          // AggregateException.cs:339-360
```

Two details are transcribed rather than reconstructed. The composition is **unconditional** when
there are leaves — this port composed only for the *default* message, which is the C2 defect. And
the trailing `sb.Length--` means the string ends at `")"` with **no** trailing space; that is
reproduced by appending `") "` and dropping the last character, rather than by special-casing the
final element, so the two spellings cannot drift apart.

The composition happens in the constructor rather than in the getter, and that is observationally
identical rather than a shortcut: `innerExceptions_` is fixed at construction and never mutated,
here and in .NET, where `_innerExceptions` is a `ReadOnlyCollection` assigned once. Composing on
access would additionally force `getMessageProperty()` to stop returning `const std::string&` —
a public signature change this needs no part of.

## 4. `Flatten` and `Handle` differ, and that is .NET's doing

```csharp
// Flatten
new AggregateException(GetType() == typeof(AggregateException) ? base.Message : Message, …)   // :335
// Handle
throw new AggregateException(Message, unhandledExceptions.ToArray(), …)                       // :281
```

`Flatten` passes the **raw** message for a plain `AggregateException` and the **composed** one for
a derived type. That discrimination is precisely what stops repeated flattening from accreting
leaf text — the pathology the ticket measured. `typeid` is the C++ counterpart of `GetType()`, and
this class is polymorphic, so it reads the *dynamic* type as .NET's does.

`Handle` passes the **composed** message. So a rethrown aggregate's message contains the leaves
twice: `"custom outer (a) (b) (a) (b)"`. That is .NET's behaviour, not an accident here, and the
two are transcribed separately rather than sharing a helper that would hide the difference.

## 5. To migrate

Rebuild. If you assert on an `AggregateException`'s message and supplied your own, expect the
leaves to follow it.

## 6. Evidence

| Mutation | Caught |
|---|---|
| `Flatten` passes the composed message (the unbounded-growth pathology) | ✅ (2 tests) |
| `Handle` passes the raw message (Flatten's rule, not its own) | ✅ |
| Compose only for the default message (the pre-#2309 C2 behaviour) | ✅ (4 tests) |
| Keep the trailing space (drop the `pop_back`) | ✅ (4 tests) |
| The separator loses its space | ✅ (4 tests) |

## 7. A pre-existing flake this ticket's gate run caught

`StopwatchDefinedArithmeticTests.Fix2326_TheResolutionThatWasLost` took 200 pairs of back-to-back
timestamps and required the smallest positive delta to be under 100 units. **That measures the
machine, not the code**: it passed eight times out of eight in isolation and failed once inside a
full gate, where any of the 200 pairs can be preempted.

The property is a **unit**, so it is now asserted against a unit: `GetTimestamp()` samples
`steady_clock`'s own epoch, so a division by 100 would put its value two orders of magnitude below
the same clock read directly. That is a comparison of values, immune to scheduling — and it still
catches the mutation that restores the division.

## 8. Downstream

Neither `cna` nor `mobile-eggbert` references `AggregateException` — zero sites in both. The
rebuild requirement is recorded here for any future consumer.
