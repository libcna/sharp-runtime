<!-- SPDX-License-Identifier: MIT -->

# CrashReason and UnitySerializationHolder — SR-AUD-127 / SR-AUD-137 review

Tickets: **#2279** (review), **#2280** (SR-AUD-127 implementation),
**#2281** (SR-AUD-137, `needs_user`), **#2282** (a separate diagnostic defect
found while reviewing, no `SR-AUD-*` identifier). Date: 2026-08-11. The audit
numbering stays frozen at 364 and **no CCF was minted**.

---

## 1. Why these two were looked at together

An inherited ranking put them in one slice with the note «no first-party
consumer». That is a property, not a cause, and it is not a reason to treat two
findings as one unit. This review re-measured the property and then asked the
only question that matters for scheduling: does either have a repair that is
compatible and unblocked?

## 2. Verdict — not a family, and they split

**Two independent findings that share a shape, not a cause**, in the sense
#2270 and #2267 established: a shared characteristic is adjacency until a single
mechanism is shown to produce both.

| | SR-AUD-127 `CrashReason` | SR-AUD-137 `UnitySerializationHolder` |
|---|---|---|
| Cause | a .NET-`internal`, nested enum was republished **verbatim** as a public top-level `System::` name, and the header calls it a ".NET counterpart" | a .NET-`internal` class was republished with a **different** public shape — public constant, raw `(intcs, string)` constructor, both field getters, argument-less methods — and the header claims that shape is "preserved for source-compatibility" |
| Extra defect the other does not have | none | the published surface lets an ordinary caller fabricate a holder state .NET only ever builds from a serialization stream, and the header's source-compatibility claim is false |
| Repair the finding itself sanctions | **two alternatives**, one of which is documentation/designation | **two alternatives, both closed** — see §5 |
| Disposition | **remediated** (#2280) | **confirmed**, approval boundary (#2281) |

Neither repair needs the other, and neither shares code with the other. They are
also not a *pair* in any privileged sense: the same "public shape drift with no
production consumer" characteristic runs through at least SR-AUD-124, -125,
-126, -128, -129 and -136 in the same module. Minting a CCF over that set would
need authority this batch does not have (CCF-021 and CCF-022 remain unminted),
and it would in any case group by characteristic rather than by cause.

## 3. Consumer inventory — measured, not inherited

`grep` over `modules/`, `test/`, `tests/` and `bench/` for each identifier:

| Type | Production consumers | Test consumers |
|---|---:|---|
| `System::CrashReason` | **0** | `modules/core/tests/System/CrashReasonTests.cpp` only |
| `System::UnitySerializationHolder` | **0** | `UnitySerializationHolderTests.cpp` (7, suite `UnitySerializationHolderTest`) and `SystemTypesRemainingTests.cpp` (8, suite `UnitySerializationHolder**Tests**`) |

The inherited "no first-party consumer" note is confirmed for both. Two things it
does not imply, and neither was assumed here: that the public surface may be
withdrawn without approval — downstream consumers exist and this batch is
forbidden from inspecting them — or that the two findings share a cause.

The `UnitySerializationHolder` duplication is worth recording because the audit
metadata flagged it: the type is covered by **two** suites whose names differ only
by a trailing `s`, in two files, with overlapping cases. That is pre-existing and
is not touched here; it belongs to whatever change eventually settles the type's
shape.

---

## 4. SR-AUD-127 — remediated by the finding's own second option

The finding closes with:

> Move it into an internal NativeAOT crash-diagnostic implementation **or**
> explicitly designate/document it as a project-specific public diagnostic type
> rather than calling it a .NET counterpart.

The first alternative withdraws a published `System::` header from the
`Core.Base` include tree — a public source break, and therefore an approval
boundary this batch does not cross. **The second was taken**, and it is not a
consolation prize: the finding's substantive complaints are that the header
"promises stable consumer access where .NET intentionally makes none" and
"provides no visibility or NativeAOT-only diagnostic explaining why an internal
runtime concept is exposed from Core.Base". Both are statements about what the
header says, and both are now false.

What changed, precisely:

- the doc-comment no longer calls the type a "C++ counterpart of .NET
  System.CrashReason"; it states that .NET publishes no such type, that the
  counterpart is the `internal` nested `System.CrashInfo.CrashReason`, and that
  this enumeration is published **on this project's own authority** so that a
  later change to .NET's internal enum is not by itself a defect here;
- it records that there is no first-party production consumer, so the constants
  are a porting vocabulary rather than a runtime contract;
- it records why the type stays published rather than becoming internal — the
  header is already independently includable and withdrawing it would break
  sources this repository may not inspect;
- it records that the enumeration is not closed over its underlying type.

**What did not change: nothing at all in the compiled surface.** No enumerator,
no value, no underlying type, no name, no header include set. The change is
confined to a comment block, so there is no ABI, layout, vtable, `noexcept`,
symbol or component-dependency consequence to assess.

### 4.1 The tests the finding asked for

The audit recorded "No invalid-value or full pairwise-distinctness vector
exists", and that was accurate: the five pre-existing tests check each enumerator
once and compare **two of the six** pairs, so two enumerators could have collided
without any test noticing. Four tests were added (5 → 9), none retired:

| Test | What it pins |
|---|---|
| `EveryPairOfEnumeratorsIsDistinct` | all six pairs, not two |
| `TheEnumeratorsAreContiguousFromZero` | the 0..3 block as a whole |
| `TheUnderlyingTypeIsIntAndTheEnumerationIsScoped` | `int` underlying type, scoped, no implicit conversion |
| `AValueOutsideTheEnumeratorsIsRepresentableAndMatchesNoEnumerator` | the doc-comment's non-closure statement, for `4` and `-1` |

Two mutations, both caught, each by the new tests as well as an old one:
making `EnvironmentFailFast` collide with `UnhandledException` fails
`EveryPairOfEnumeratorsIsDistinct` and `TheEnumeratorsAreContiguousFromZero`;
moving `InternalFailFast` to `4` fails `TheEnumeratorsAreContiguousFromZero` and
`AValueOutsideTheEnumeratorsIsRepresentableAndMatchesNoEnumerator`.

Sanitizers are not applicable: an enumeration's constants and a comment have no
runtime behaviour to instrument.

---

## 5. SR-AUD-137 — an approval boundary, preserved

The finding closes with:

> Either make the wrapper internal **and** explicitly project-specific, or retain
> recognizable compatibility signatures and provide deterministic unsupported
> diagnostics at those boundaries.

Unlike SR-AUD-127's, **neither alternative is available to this batch**, and the
difference is not a matter of degree:

- The first is a conjunction. "Explicitly project-specific" is documentation and
  is free; "make the wrapper internal" is a public source break — it withdraws
  `System::UnitySerializationHolder`, or at minimum its public constant,
  constructor and two getters, from the `Core.Base` include tree. Doing the
  documentation half alone does not satisfy the option as written.
- The second requires `SerializationInfo` and `StreamingContext` parameters on
  the constructor, `GetObjectData` and `GetRealObject`. Neither type exists in
  this port, and **`CLAUDE.md` records serialization infrastructure
  (`[Serializable]`, `SerializationInfo`) as a permanent deviation** — "ignored;
  not needed for game code". Implementing it would reverse a standing project
  decision, which is a user decision, not a remediation.

So SR-AUD-137 stays **confirmed**, and ticket **#2281** (`needs_user`) carries the
choice with both costs stated. It is deliberately *not* promoted merely because
it was ranked next to a finding that turned out to be repairable.

### 5.1 What was done anyway, and what it does not close

One thing in the header is wrong independently of which option is eventually
chosen. It states:

> The public API surface — the NullUnity constant, GetRealObject(), and
> GetObjectData() — is preserved for source-compatibility.

That is false, and the audit's own comparison shows why: in .NET the type is
`internal`, `NullUnity` and both fields are private, and all three members take
`SerializationInfo`/`StreamingContext`. Nothing about the port's shape is
source-compatible with that; C# code written against .NET's signatures does not
compile against this header. Leaving a false parity claim in a public header is a
maintainability defect on its own terms, and correcting it is compatible under
any outcome of #2281.

The doc-comment was therefore rewritten to state what the port actually provides,
why the serialization interfaces are absent, that an ordinary caller can fabricate
a holder state .NET only builds from a stream, and that the shape is under an open
decision. **This does not close SR-AUD-137** — the surface it describes is
unchanged — and the audit report and index row say so explicitly.

No test was added to `UnitySerializationHolder`. Pinning more of a public shape
that #2281 may withdraw would raise the cost of the very decision the ticket
exists to take, and the 15 existing tests already cover the current behaviour.

### 5.2 A separate defect found while reviewing (#2282, no SR-AUD identifier)

`GetRealObject()` composes its rejection message as

```cpp
throw ArgumentException("Invalid unity type: " + (data_.empty() ? std::to_string(unityType_) : data_));
```

so whenever the optional data string is non-empty the message prints **the data,
not the unity type it names**: `UnitySerializationHolder(999, "UnknownType")`
reports `Invalid unity type: UnknownType`, and the value 999 that actually caused
the rejection never appears. That is a diagnostic defect verifiable entirely
inside this repository, independent of any .NET text.

It is **not** folded into SR-AUD-137, which is about the public shape, and it is
**not** implemented here: any repair changes the message text, and the audit
already recorded that "the exact invalid-unity diagnostic and `ArgumentException`
parameter/context are not compared with the reference behavior" — that comparison
needs the .NET reference, which is unavailable (`/rv` absent). #2282 is filed
`todo` at P3 with both halves separated: the misleading insertion is measured, the
exact replacement text is deferred.

---

## 6. Disposition summary

| Finding | Before | After | Ticket |
|---|---|---|---|
| SR-AUD-127 | confirmed | **remediated** | #2280 |
| SR-AUD-137 | confirmed | **confirmed** (unchanged; approval boundary recorded) | #2281 `needs_user` |
| — | — | new ordinary defect | #2282 `todo` |
