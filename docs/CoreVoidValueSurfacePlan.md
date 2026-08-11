<!-- SPDX-License-Identifier: MIT -->

# `System::Void` — SR-AUD-136 review and repair

Tickets: **#2285** (review), **#2286** (implementation). Date: 2026-08-11.
The audit numbering stays frozen at 364, **no new `SR-AUD-*` identifier was
created** and **no CCF was minted**.

---

## 1. The frozen finding, quoted

> **SR-AUD-136 — medium — Void is presented as a normal C++ value/generic type
> although C# forbids the documented use cases**
>
> The header says its ordinary struct permits porting generic patterns such as
> `Nullable<Void>` and `Task<Void>`, and it adds instantiation, equality, and an
> empty-string `ToString`. C++ tests construct values, place them in vectors, and
> assign them to Nullable. The local C# compiler rejects all analogous ordinary
> `System.Void` uses with CS0673: the public runtime metadata type represents the
> `void` return type, not a usable C# value or generic argument. The same source
> has no declared fields or methods, while this header creates a user-observable
> value API.
>
> If a C++ unit/absence type is needed, make it a clearly project-specific type
> and remove the claimed C# generic portability. If reflection compatibility is
> the goal, retain only metadata-oriented handling and document that ordinary
> construction, equality, and value text are C++ extensions.

The report's reproduction basis is a local C# probe (construction, `ToString`,
`List<System.Void>`) failing with **CS0673**, construction additionally with
**CS0143**, read against local .NET `System/Void.cs:8-12`. `/rv` is absent in
this environment, so that frozen record — not a fresh reference read — is the
authoritative .NET evidence used below. Nothing here rests on inventing .NET
text.

## 2. Verdict on the inherited "same shape as SR-AUD-127" ranking

**The analogy is superficial and the root cause is different.** Both findings
are about what a header *says*, and both types have no first-party consumer, but
that is the characteristic the previous review (#2279) already refused to treat
as a cause — it also runs through SR-AUD-124/125/126/128/129/137.

| | SR-AUD-127 `CrashReason` | SR-AUD-136 `Void` |
|---|---|---|
| Does .NET publish the name? | **No.** The counterpart is the `internal`, nested `System.CrashInfo.CrashReason` | **Yes.** `System.Void` is a public .NET type |
| Cause | a .NET-`internal` concept republished as a public top-level `System::` name that calls itself a ".NET counterpart" | a **public** .NET *metadata* type given a **value API** and a porting rationale describing C# source that **cannot compile** |
| What the repair has to correct | the claimed provenance | the claimed *use model*, in both directions (§4) |
| Compatible route | the finding's **second** alternative | the finding's **first** alternative |

The one-line consequence: for `CrashReason` the compatible route was to stop
calling a project-owned name a .NET counterpart; for `Void` the name and the
field-less shape genuinely *are* .NET's, and what had to stop being claimed is
that C# code exists which this value API helps port. Repairing either tells you
nothing about the other. No CCF.

## 3. Measured state

### 3.1 Public shape (`modules/core/include/System/Void.hpp`, pre-change)

An aggregate empty struct with three user-declared members —
`std::string ToString() const`, `bool operator==(const Void&) const noexcept`,
`bool operator!=(const Void&) const noexcept` — plus the implicitly declared
default/copy/move constructors, assignments and destructor. Header-only, no
`.cpp`, one standard include (`<string>`). Independently includable from the
`Core.Base` include tree.

### 3.2 Consumer inventory — measured, not inherited

`grep` for `Void` over `modules/*/include`, `modules/*/src`, `bench/`, `test/`
and `tests/`:

| Kind | Count | Sites |
|---|---:|---|
| First-party production consumers | **0** | none — no declaration, return type, member or local anywhere outside the header |
| Test consumers | 2 files, 10 cases | `modules/core/tests/System/VoidTests.cpp` (suite `VoidTest`, 5) and `modules/core/tests/System/SystemTypesRemainingTests.cpp` (suite `VoidTests`, 5) |

Both suites link into `SharpRuntimeTests_Core_Base`. The duplication — two suites
whose names differ only by a trailing `s`, with overlapping cases — is the same
pre-existing pattern the previous review recorded for `UnitySerializationHolder`,
was already flagged in this file's audit metadata, and is **not** touched here.

Zero production consumers does **not** license withdrawing the header or any
member: downstream consumers exist and this batch may not inspect them.

### 3.3 Tests, before

`DefaultConstruct`, `ToStringEmpty`, `EqualityAlwaysTrue`,
`InequalityAlwaysFalse`, `UsableAsTemplateArg` (a `std::vector<Void>`) in the
owning file; `DefaultConstructible`, `Equality_TwoInstances_AreEqual`,
`ToString_ReturnsEmpty`, `UsableAsTemplateArgument` (a `Nullable<Void>`) and
`IsTrivial` (`sizeof(Void) == sizeof(unsigned char)`) in the aggregate file.

## 4. Premise correction — the doc-comment was wrong in *two opposite* directions

The finding names the over-claim. Measuring the header turned up a second,
independent falsehood in the same comment, pointing the other way:

> `@note You cannot use System::Void in C++ code directly as a variable type in
> the same way as @c void, but you can use it as a template argument where a
> concrete type is required.`

`System::Void v;` compiles, and the header's own test `VoidTest.DefaultConstruct`
has been proving so since the type was written. The note is a transliteration of
the C# CS0673 restriction into a language that does not impose it. So the header
simultaneously

1. **over-claimed** — it offered C# generic portability (`Nullable<Void>`,
   `Task<Void>`) for source that C# refuses to compile, and
2. **under-claimed** — it denied a C++ capability the type has and the suite
   exercises.

Both are repaired by the same edit. This is recorded as a premise correction
inside SR-AUD-136 rather than as a new ticket: it is the same defect class in the
same doc-comment, closed by the same change, so a separate ticket would track
nothing. (Contrast #2282, which is a *diagnostic message* defect in a different
type and survives its finding's repair.)

A third, smaller inaccuracy was corrected in passing: `ToString`'s comment said
"System.Void has no documented ToString in .NET", which understates the frozen
evidence — .NET's struct declares **no member at all**, and C# cannot obtain a
value on which to call one.

## 5. The repair, and why it needs no approval

The finding offers two alternatives. Alternative two — "retain only
metadata-oriented handling" — is not available: reflection is a permanent project
deviation (`CLAUDE.md`, *Parity philosophy*), so there is no metadata handling to
retain, and stripping the type back to it would delete three public members.

Alternative one is available in full and needs no compiled-surface change:

> "make it a clearly project-specific type and remove the claimed C# generic
> portability"

with one honest refinement recorded here rather than glossed over. The finding's
phrase "make it a clearly project-specific type" cannot be taken wholesale,
because — unlike `CrashReason` — `System.Void` **is** a public .NET type and the
empty field-less shape **is** .NET's. What is project-specific is the *use
model*: the value API and every position C# forbids. The header now says exactly
that, no more.

The doc-comment states, after #2286:

- what .NET's `System.Void` is — a public *metadata* type naming the `void`
  return type, declaring no field;
- that C# rejects construction (CS0143) and rejects the type as a value or
  generic argument (CS0673), and names the type object with `typeof(void)`;
- that **no compiling C# source therefore exists** for the `Nullable<System.Void>`
  / `Task<System.Void>` patterns, so the header no longer claims to help port
  them;
- that in C++ the type may be declared, copied, compared, stored and passed as a
  template argument — every one of which C# forbids — and that this is the
  project's adaptation;
- that `ToString()`, `operator==` and `operator!=` are extensions with no .NET
  counterpart, and that the empty string is a project decision, not a parity
  value;
- that this port's `Void` has no base class, hence no inherited object text, no
  `GetHashCode`, no `Equals(object)`, no ordering and no `std::hash`;
- that the only property shared with .NET is the compiler-visible shape;
- that there is no first-party production consumer, and why the header and all
  three members stay published anyway.

Both of the finding's substantive complaints are statements about what the header
presents, and both are now false: it no longer presents the type as supporting
C# generic porting, and the value API is no longer presented as anything but a
C++ extension.

### 5.1 What was deliberately **not** done

Removing `ToString`/`operator==`/`operator!=`, or withdrawing `System/Void.hpp`,
would be a public source break — an approval boundary this batch does not cross,
for the same reason as #2280. Nothing in the finding requires it: its own
alternative one keeps the members and repairs the claim. Should that decision
ever be revisited, it needs its own approval ticket; SR-AUD-136 does not remain
open pending it, because the finding as frozen is about presentation.

## 6. Tests — one added, none retired

The instruction that governed test selection was to pin project-owned behaviour
**without** hardening a surface a later approved change may need to withdraw. The
existing ten cases already pin construction, equality, `ToString`, `vector<Void>`
and `Nullable<Void>` — twice over — so adding more pins on those members would
work directly against a future withdrawal decision and was rejected.

One case was added, `VoidTest.IsAnEmptyFieldlessTagType`, which pins the single
property this port genuinely shares with .NET and which **no** existing case
covers:

```
std::is_empty_v<Void>                             // .NET declares no field
!std::is_polymorphic_v<Void>                      // no vtable; ToString overrides nothing
std::is_standard_layout_v<Void>
std::is_trivially_default_constructible_v<Void>
std::is_trivially_copyable_v<Void>
std::is_trivially_destructible_v<Void>
sizeof(Void) == 1
```

`sizeof(Void) == sizeof(unsigned char)` in the aggregate file is **not** a
substitute: a one-byte data member keeps that equality while destroying
emptiness. This pin survives any future decision about the three extension
members, which is why it was the one worth adding.

**Mutations** (rebuilt each time, `SharpRuntimeTests_Core_Base` relinked, run
before/after — see §8):

| # | Mutation | Result |
|---|---|---|
| 1 | add `char pad_ = 0;` to `Void` | **caught** — `is_empty_v` false and `is_trivially_default_constructible_v` false in the new case; `sizeof` and every pre-existing case still passed, which is the gap the new case closes |
| 2 | `ToString()` returns `" "` | caught by the two pre-existing `ToString` cases |

## 7. Compatibility

| Dimension | Effect |
|---|---|
| Public source | **none** — no member added, removed, renamed, re-signed or re-qualified |
| ABI / symbols | **none** — every member is `inline` in the header and unchanged |
| Object layout | **none** — `sizeof` 1, `alignof` 1, still empty; asserted by the new case |
| Vtable | **none** — non-polymorphic before and after; asserted |
| `noexcept` | **none** — both operators keep `noexcept`, `ToString` keeps none |
| Includes / component graph | **none** — still `<string>` only, still `Core.Base` |
| Behaviour | **none** — no executable statement was changed |

The whole change is a doc-comment rewrite plus one test.

## 8. Validation

Recorded in the ticket and the final batch report: `build/` only, `--parallel 2`,
`SharpRuntimeTests_Core_Base` rebuilt and rerun, then the complete 38-executable
gate. No sanitizer run: the defect class is *public API documentation*, and there
is no memory-safety, lifetime, arithmetic or UB question anywhere in a change
that alters no executable statement — running ASan/UBSan here would be
validation theater. Selective components were not rerun: no component boundary,
dependency, module or catalogue entry changed.

## 9. Disposition

**SR-AUD-136 → remediated** (#2285 review, #2286 implementation). The finding's
own first alternative, taken in full, with the two premise corrections in §4
recorded rather than absorbed. No source break, no new identifier, no CCF, and
the withdrawal option preserved as a future approval boundary rather than
silently foreclosed.
