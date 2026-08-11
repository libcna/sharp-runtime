<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` `ArgIterator` fixture object lifetime — plan

Tickets #2274 (review) and #2275 (implementation). One frozen audit finding in
`modules/core/include/System/ArgIterator.hpp`:

| Finding | Severity | Headline |
|---|---|---|
| SR-AUD-112 | medium | `ArgIterator` tests invoke non-static methods on storage that is not an `ArgIterator` object |

Audit numbering is frozen at `SR-AUD-001..364`; this ticket creates no new
`SR-AUD-*` identifier. This is a **singleton on one fixture's object-access
route**, not an `ArgIterator` review and explicitly not a step toward CLR
varargs, which `CLAUDE.md` records as out of scope.

---

## 1. Root cause

`ArgIterator` declares two constructors, both `[[noreturn]]`. Declaring any
constructor suppresses the implicit default one, so the type has **no way to be
constructed at all** — measured, `is_default_constructible_v` is `false` and both
public constructors throw. `Batch12ArgHandleTests.cpp` therefore fabricated an
object: an `alignas(ArgIterator) char[sizeof(ArgIterator)]`, `reinterpret_cast`
to `ArgIterator*`, and member calls through that pointer.

Calling a non-static member function through a pointer to storage in which no
object's lifetime has begun is undefined behaviour ([basic.life]), regardless of
whether the callee touches state. The methods are empty today, so the tests pass
— but they validate a fabricated object, and a nonempty stub, an optimisation, or
a toolchain change can invalidate them with no production behaviour changing.

**Premise refinement:** the finding says "five tests"; there are **six**, holding
**seven** fabricated objects — `Equals_ReturnsFalse` builds two. Enumerated:
`End_DoesNotThrow`, `GetHashCode_ReturnsZero`, `Equals_ReturnsFalse`,
`GetNextArg_Throws`, `GetNextArgType_Throws`, `GetRemainingCount_Throws`.

---

## 2. Measured before-state

`build-probe/2274_probe1_before.cpp` / `.log`:

```
sizeof=1 alignof=1
default_constructible=0
trivially_copyable=1
copy_constructible=1
aggregate=0
empty=1
ctor_from_handle=1
bit_cast_hash=0 bit_cast_equals=0
bit_cast_getremaining_threw=1
ctor1_threw=1 ctor2_threw=1
```

The type is empty, trivially copyable, non-aggregate and not
default-constructible. Every behaviour the six tests assert is reproducible
through a legitimately created object — see §3.

---

## 3. The repair

`std::bit_cast<ArgIterator>(static_cast<unsigned char>(0))`. It is constrained
only on trivial copyability and equal size, needs no constructor, and **returns
an object of the destination type** — so the lifetime the fixture was missing
actually begins. The class is empty, so there is no value representation to get
wrong.

That is the whole change on the test side: each `reinterpret_cast` of raw storage
becomes one legitimately created object, and **every existing assertion is kept
verbatim**. No production behaviour is involved, so nothing about `ArgIterator`'s
public contract moves.

Four new tests pin the facts the repair depends on, so that a future change which
invalidates the mechanism fails loudly instead of tempting the next author back
into raw storage: the type is not default-constructible (the reason the
fabrication existed at all), it is trivially copyable, it is empty, and both
constructors throw with their exact messages.

---

## 4. What the sanitizers say: nothing — measured

`build-probe/2274_probe2_sanitizer.cpp` reproduces the fixture's exact shape — an
empty, non-polymorphic class whose only constructor throws, reached through a
`reinterpret_cast` of raw character storage — and runs under
`-fsanitize=undefined,address -fno-sanitize-recover=undefined`. It completes with
**no diagnostic and exit status 0** (`build-probe/2274_probe2_sanitizer.log`).

This is expected rather than surprising: UBSan's object-lifetime-adjacent checks
(`vptr`, `object-size`) need a polymorphic type or a known allocation, and ASan
sees a legally sized, legally aligned stack buffer being read and written within
bounds. There is no instrumentation for "this pointer does not designate an
object".

So the sanitizers are **not discriminating for this defect class**, and running
them for ceremony would have produced a green result that argued the opposite of
the truth. The discriminating instrument is the source itself: the fixture either
forms a pointer to storage or it holds an object, and after this ticket it holds
an object. This is the same lesson #1836/#1837 recorded — an instrument that
enumerates undefined *operations* is silent about a wrong *construction*.

---

## 5. Mutations

`build-probe/2275_run_mutations.sh`, results in `build-probe/2275_mutations.log`.

Mutating the fixture *back* to raw storage is an **equivalent mutation by
construction** and is not run: the defect's symptom is identical observable
output, which is precisely why the tests were green for as long as they were.
Claiming a kill there would be false. What is testable is the guard the repair
added, so the mutations are applied to the production header:

| # | Mutation | Result |
|---|---|---|
| 1 | `ArgIterator` gains a default constructor | **caught** — `IsNotDefaultConstructible` fails |
| 2 | `ArgIterator` gains state, so `bit_cast`'s size precondition fails | **caught at compile time** — `no matching function for call to 'bit_cast<System::ArgIterator>(unsigned char)'` |
| 3 | the handle constructor's message changes | **caught** — the message test fails |
| 0 | control, unmutated | survived, as required |

Mutation 2 is the one that matters: it demonstrates the claim in §3 that a change
invalidating the mechanism fails **loudly**, at compile time, rather than
silently reverting the fixture to fabricating an object.

---

## 6. Compatibility

| Property | Change |
|---|---|
| production source | **none** except the header doc-comment in §7 |
| public signatures, layout, vtable, `noexcept`, symbols | none |
| test count | +4 (6 rewritten in place, 0 removed) |

---

## 7. Residual, deliberately not answered here

No public construction of `ArgIterator` can succeed, so `End`, `Equals`,
`GetHashCode`, `GetNextArg`, `GetNextArgType` and `GetRemainingCount` are
**unreachable through the public API**. This ticket makes the fixture stop
lying about *how* it reaches them; it does not decide whether they should be
reachable.

The audit raises exactly this ("End/equality/hash/next-argument methods need a
deliberate static or unsupported-operation design"), and answering it means
choosing among: giving `ArgIterator` a default constructor, so a default value
exists as it does for the .NET value type — a public API addition; making the
unreachable members `static` or removing them — a public source break; or
recording the unreachability as the intended end state for an explicit
`NotSupportedException` stub. The header now **documents** the unreachability so
it is at least visible, and **#2276 (`needs_user`)** carries the choice.

---

## 8. Ticket map

| Ticket | State |
|---|---|
| #2274 | review — done |
| #2275 | implementation — done, SR-AUD-112 `remediated` |
| #2276 | `needs_user` — should `ArgIterator`'s unreachable instance members become reachable, static, or stay as they are? |
