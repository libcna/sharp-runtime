<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Five independently reviewed `modules/core` findings (#2317 ownership pool, part 1)

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

---

# The remaining seven `modules/core` findings (#2317 ownership pool, part 2)

Reviewed 2026-08-12, in the order #2317 records them. **These seven are not a family
either**, and they are not a family with the five above. Seven headers, seven root causes.
Three of them turned out to be conjunctions with a genuinely compatible clause, which was
implemented; four are decisions or blocked on data. Nothing here mints or extends a CCF, and
no `SR-AUD-*` identifier is created; numbering stays frozen at 364. **All seven stay
`confirmed`.**

| Finding | Header | Root cause | Review/impl. ticket | Forward ticket | State |
|---|---|---|---|---|---|
| SR-AUD-053 | `Array.hpp` | a public `constexpr` value the port already contradicts in two other headers | #2327 | — | `needs_user` |
| SR-AUD-055 | `ArraySegment.hpp` | a copy destination that grows instead of being rejected — the finding names the choice | #2328 | #2329 (`done`, doc) | `needs_user` |
| SR-AUD-063 | `Tuple.hpp` | public mutable fields where .NET is immutable; every repair is a source break | #2330 | — | `needs_user` |
| SR-AUD-069 | `SequencePosition.hpp` | **conjunction**: a missing value contract (additive) + a public representation (break) | #2331 (`done`) | #2332 | `needs_user` |
| SR-AUD-110 | `RuntimeType.hpp` | **conjunction**: a false counterpart claim (doc) + an occupied .NET name | #2333 (`done`) | #2334 | `needs_user` |
| SR-AUD-173 | `Globalization/CharUnicodeInfo.hpp` | **conjunction**: an undeclared reduction (doc) + a missing Unicode numeric table | #2335 (`done`) | #2336 | `blocked` |
| SR-AUD-182 | `StringNormalizationExtensions.hpp` | **conjunction**: unpinned gated behaviour (tests) + absent normalization tables *and* algorithm | #2337 (`done`) | #2338 | `blocked` |

**Approval F is consumed twice, and only as a gate.** SR-AUD-173's table clause (#2336) and
SR-AUD-182's implementation clause (#2338) are both blocked behind the *same* open decision
already before the user as **Approval F / #2018** — a Unicode data source, its attribution,
and a stated Unicode version with an update policy. Neither opens a new approval question.
They are **not one implementation cause**: #2336 needs the `Numeric_Type`/`Numeric_Value`
fields, #2315 (SR-AUD-174) needs the general-category field, #2018 needs category plus simple
case mapping, and #2338 needs canonical and compatibility decomposition mappings, canonical
combining classes and composition exclusions **plus a full UAX #15 implementation** — the
only one of the four that is not a table lookup. One gate, four different payloads.

---

## SR-AUD-053 — `Array::MaxLengthProperty()` is `INT32_MAX` (#2327, `needs_user`)

**Live state.** `Array.hpp:38` — `static constexpr intcs MaxLengthProperty() noexcept
{ return std::numeric_limits<intcs>::max(); }`, i.e. **2,147,483,647**, where .NET's
`Array.MaxLength` is `0x7FFFFFC7` = **2,147,483,591**. The gap is 56.

**Measured consumer surface: nothing uses it.** Across `modules/`, `tests/`, `test/` and
`bench/` the name appears exactly twice outside its own definition — the audit index and
`ArrayTests.cpp:386`, `EXPECT_GT(Array::MaxLengthProperty(), 0)`. No production call site, no
`static_assert`, no array sizing, no serialized form, no allocation or indexing path validates
against it. Changing the value would break no first-party build and no first-party test.

**Premise addition the finding does not state: the port already disagrees with itself.**
Two other headers carry `0x7FFFFFC7` **as .NET's `Array.MaxLength`**, and one of them is
public: `System::Buffers::MemoryPool<T>::MaxArrayLength` (`MemoryPool.hpp:42`, a public
`static constexpr intcs` documented "Matches .NET's Array.MaxLength (Array.cs), the ceiling
MaxBufferSize/Rent() validate against", and pinned to the exact literal by
`MemoryPoolTests.MaxBufferSize_MatchesArrayMaxLength`), and
`ArrayBufferWriter<T>::MaxArrayLength` (`ArrayBufferWriter.hpp:62`, private). So the .NET
value is already the repository's answer everywhere the limit is actually *enforced*; the
only place that publishes a different one is the property named after the .NET constant,
where it is enforced nowhere.

**Why it is still a decision and not a one-line fix.** The value is a **public `constexpr`**
in a shipped header. Lowering it is observable in the source domain (a downstream
`static_assert` or `constexpr` context changes answer) and in the runtime domain (a downstream
`if (n > Array::MaxLengthProperty())` guard newly rejects the 56 values 2,147,483,592 through
2,147,483,647). That surface cannot be measured from here — CNA and mobile-eggbert are outside
this repository's boundary — and the repository's own precedent for a public constant whose
value or unit changes is `needs_user` (SR-AUD-130 / #2326, decided one day earlier). The
finding's own remediation sentence is a **disjunction** — "Align `MaxLengthProperty` with .NET
**or** document a deliberate vector-runtime adaptation and test the exact value" — which is
the same shape as `CLAUDE.md`'s porting checklist item 5 ("constants ... match the .NET source
where applicable. Discrepancies must be either fixed or explicitly documented as intentional
deviations"). Repository authority frames the choice; it does not make it.

**Deliberately not done: pinning the current value.** The finding notes that the positivity
test "cannot detect the mismatch", and a test asserting `== INT32_MAX` would fix that. It is
not added, because pinning the un-decided branch as the tested contract pre-empts the
decision; the ticket that sets the value sets its test in the same change. Both options are
priced in #2327.

## SR-AUD-055 — `ArraySegment<T>::CopyTo` resizes the destination (#2328 `needs_user`, #2329 `done`)

**Live state.** `ArraySegment.hpp:287-303`. `CopyTo(std::vector<T>&, intcs destinationIndex)`
rejects the default segment and a negative index, then computes `needed = destinationIndex +
count_` in `longcs` and calls `destination.resize(needed)` whenever the destination is
shorter. The one-argument overload forwards to it with index 0. The sibling
`CopyTo(ArraySegment<T>&)` **rejects** a short destination with
`ArgumentException("Destination ArraySegment is too short.")`.

**Premise correction: four tests pin the resize, not one.** The finding names
`CopyTo_VectorWithOffset_ExpandsDest`. Measured live, the behaviour is also pinned by
`ArraySegmentTests.CopyTo_Vector_CopiesAllElements` and
`ArraySegmentTests.CopyTo_Vector_PartialSegment` — both copy into an **empty** `dest` through
the one-argument overload, which the finding treats only as "forwards to that behavior" — and
by `CoreMemorySafetyOverlapTests.NonOverlappingCopiesKeepTheirPreviousResults`
(`CoreMemorySafetyTests.cpp:837-838`), whose own comment reads `// SR-AUD-055's resize,
unchanged`. That last one matters for sequencing: ticket #2214 repaired this file's default-
state and overlap defects and **deliberately preserved** the resize while doing so.

**Premise addition: the repository has a convention, and this is the one place that breaks
it.** Of the 26 `void CopyTo(std::vector<...>&, ...)` overloads in `modules/`, **20 reject a
short destination** — 13 with their own guard (`List`, `LinkedList`, `Collection`,
`ReadOnlyCollection`, `FrozenSet`, `FrozenDictionary`, the three `Concurrent*`,
`StringCollection`, `OidCollection`, both `Colors`), five through the shared
`System::Collections::detail::requireValidCopyDestination` helper (`Hashtable`, `Stack`,
`Queue`, `ListDictionaryInternal`, `ICollection`), and `ImmutableList`'s two forwarding forms
through its four-argument body. `ArraySegment`'s two-argument overload is the **only** one
that grows a caller-sized destination to make room. (`BitArray`'s two forms *replace* the
destination outright — a third shape, a different .NET member, and not this finding.) Note the
boundary: the shared helper lives in `Collections`, and `modules/core` may not depend on it,
so a repair here writes its own two-line guard rather than reusing it.

**Why it is a decision.** The finding's remediation sentence *is* the decision — "Decide
whether the vector adaptation intentionally permits resizing; if not, reject short capacity
before mutation, and if it is retained, document it prominently as a breaking deviation". The
reject branch is a **runtime behaviour break on currently accepted input**: calls that
succeed today start throwing, and four first-party tests must be inverted. This repository
escalates that class (the `Decimal` group separator, the four date/time parsers and the
`String::Format` grammar all needed explicit approval and a recorded migration). Both options
are priced in #2328.

**Compatible clause, implemented (#2329, `done`).** The one-argument overload's doc-comment
was **factually wrong**: it required the destination to "have capacity for at least Count
elements" and said elements are "written via push_back / assignment into existing slots".
There is no `push_back` anywhere in the file, no capacity requirement, and the two-argument
overload it forwards to documents the exact opposite ("automatically resized (never throws for
insufficient room)"). Correcting a false statement is not choosing a branch: the new text
states what the code does today and says in one sentence that whether the resize stays is
SR-AUD-055's open question. Documentation only; no code, no test, no signature changed.

## SR-AUD-063 — `TupleN` fields are public and mutable (#2330, `needs_user`)

**Live state.** All eight arities are `struct`s with public `ItemN` data members (`Tuple8`
also publishes `Rest`); `Tuple::Create(1, 2).Item1 = 99` compiles and sticks, exactly as the
audit probe reported. .NET's `TupleN` holds private `readonly` fields behind getter-only
properties.

**Measured consumer surface.** No first-party code **writes** an `ItemN` — the assignment
form `\.Item[1-8] *=` has zero matches outside `ValueTuple`. But 75 first-party sites **read**
one: `TupleTests.cpp` (33), `TupleNewTests.cpp` (26) and `SystemTypesRemainingTests.cpp` (16).
Every one of those breaks under the only repair that matches .NET, because the port's naming
rule turns `t.Item1` into `t.getItem1Property()`. `ValueTuple` is not affected and must not be
changed with it: .NET's `ValueTuple` fields really are public and mutable, so its 52 sites are
correct parity, not the same defect.

**Premise addition: the cheap-looking shortcut is the expensive one.** "Just make the members
`const`" is not a smaller version of this repair. A class with a `const` non-static data
member has its implicitly declared copy-assignment operator defined as deleted, so
`t = Tuple::Create(...)`, `std::vector<Tuple2<...>>::push_back`, `std::sort` over tuples and
every other assignable-value use stop compiling — a **wider** break than the getter migration,
and one whose diagnostics point at the standard library rather than at the change. No
first-party site does any of that today, which is precisely why the cost would land entirely
downstream and unmeasured.

**Why it is a decision.** Every route to .NET's contract — private fields plus
`getItemNProperty()`, or `const` members — is a public source break in a shipped header, and
the audit index already records this finding as "a public source break needing approval" (see
the SR-AUD-011 remediation note, which excludes it on exactly that ground). Same class as
SR-AUD-068/#2322. Options priced in #2330.

## SR-AUD-069 — `SequencePosition` (#2331 `done`, #2332 `needs_user`)

**This finding is a conjunction of three clauses**, and the audit's own text separates them:
the mutable public representation, the missing `Equals`/`GetHashCode` contract, and the
missing tests. Only the first needs approval.

**Live state before #2331.** A `struct` with public `void* object_` and `intcs integer_`,
public `GetObject()`/`GetInteger()`, and `operator==`/`operator!=` — no named `Equals`, no
`GetHashCode` at all.

**Measured consumer surface: the migration cost of the break is zero, first-party.** Across
the repository the type appears in seven files (`ReadOnlySequence.hpp`, `SequenceReader.hpp`,
`BuffersExtensions.hpp`, three `modules/buffers` suites, and its own header). **Direct field
access exists in exactly one place — inside `SequencePosition` itself.** Everything else goes
through the constructor and the two getters, which are already the migration target. The two
closest siblings in the same module settle the convention: `System::Index` and `System::Range`
are `class`es with private fields and public `Equals`/`GetHashCode`.

**Clause implemented (#2331, `done`), purely additive.** `Equals(const SequencePosition&)`
and `GetHashCode()` are added; `operator==`/`!=` now delegate to `Equals`, so the named and
operator forms cannot drift apart. The hash is `((h1 << 5) + h1) ^ h2` over the folded pointer
bits and the integer, evaluated in `uintcs` — signed overflow would be undefined behaviour
here, the CCF-004 class already recorded for `detail::tupleHashCombine` — and the pointer fold
is guarded by `if constexpr (sizeof(std::uintptr_t) > sizeof(uintcs))`, because `bits >> 32`
on a 32-bit target is a shift at the operand width, not a zero. **It is not .NET's hash
value**, and the header says so: .NET combines the segment object's managed hash with the
integer, and neither the managed object nor a readable copy of that combiner exists here
(`/rv` absent). This is *not* the unverifiable-reference-text class of #2321/#2323: a hash
value is documented as unstable in .NET too, and this port already ships two hashes that
deliberately differ from .NET's (`Range::GetHashCode`'s `h1 ^ (h2 * 397)`, and
`System::HashCode` mixing a `std::random_device` seed per process). What a caller may rely on
— equal values hash equally — is exact and tested.

**+7 tests** in `Batch6BuffersTests.cpp`, closing the finding's own list: equal and unequal
**non-null** segment pointers (every pre-existing direct test used `nullptr`), default
equality, hash agreement for equal values, hash stability, and the caveat that component
equality is not sequence-location identity. They obey `docs/HashAssertionContractRule.md`: the
contract direction is asserted (R1), no "unequal values hash differently" pair is added (R2),
dependence on the segment component is stated over a **family** of eight pointers rather than
one pair, and the one exact pin is admitted by R3 and says which property it pins — with a
null segment the folded half is zero and the hash reduces to the integer's own bit pattern,
which is the only reproducible part of it.

**Three mutations, three caught**, each rebuilt and re-executed against
`SharpRuntimeTests_Buffers`: a hash that ignores the integer fails the R3 pin, a hash that
ignores the segment fails the family test, and an `Equals` that compares only the integer
fails the non-null-segment test. Two earlier spellings of the first two mutations were
**discarded, not counted**: dropping the operand outright left an unused variable and
`-Werror` rejected the build, so each was re-expressed as a zeroed input. The header was
restored byte-identical afterwards and the suite is green at 13.

**Clause deferred to #2332 (`needs_user`).** Making `object_`/`integer_` private is a public
source break: it removes structured bindings, designated initialisers and direct assignment
for any downstream consumer that uses them. Layout is unaffected — all members keep the same
access, so the class stays standard-layout and `sizeof`/`alignof` do not move — so this is
source compatibility only, and its first-party cost is nil. The header now documents the
divergence and says the decision is open; it does not take it.

## SR-AUD-110 — `System::RuntimeType` (#2333 `done`, #2334 `needs_user`)

**Live state.** A six-value public `enum class` — `None`, `Primitive`, `ValueType`,
`ReferenceType`, `Array`, `GenericParameter` — whose doc-comment claimed it was the "C++
counterpart of the internal .NET System.RuntimeType enumeration" and that "The values here
match the documented internal constants used in CoreCLR". .NET's `System.RuntimeType` is an
internal sealed **class** deriving from `TypeInfo`; there is no such enumeration and there are
no such constants. Both claims were false.

**Measured consumer surface: zero, including tests of anything else.** Exactly one file in
the repository includes the header — `modules/core/tests/System/RuntimeTypeTests.cpp` — and
its six tests assert the six integer values. No production translation unit mentions the type.
(The 105 repository hits for the substring are `RuntimeTypeHandle`, an unrelated type.)

**Premise correction: the finding's stated harm is mostly moot under repository policy.**
The audit's reason is that the collision "prevents a future reflection-compatible runtime-type
port from using its source name". `CLAUDE.md` lists reflection — `System::Type`,
`System::Activator`, `Enum.GetNames/GetValues` and the rest — as **completely out of scope, a
permanent deviation whose stubs are the correct end state**. The .NET class whose name this
occupies is therefore never going to be ported here and will never need the name back. What
survives the correction is the second half of the finding: the type "lets C++ callers depend
on a semantic surface that has no .NET counterpart", which is a real exposure regardless of
the name.

**Clause implemented (#2333, `done`), documentation only.** The two false claims are
withdrawn in the header and replaced with what is true: no .NET counterpart, invented values,
no production consumer, the single test file that pins the integers, and the fact that
reflection's permanent-deviation status removes the name-reclamation argument. No code, no
value, no test changed.

**Clause deferred to #2334 (`needs_user`).** Whether the type is kept as a documented
port-local classifier, renamed out of the `System::RuntimeType` name, or removed is a public
type-identity decision: renaming or removing a published `enum class` is a source break for
any downstream `#include`/use, and this repository cannot see its downstream. Zero first-party
cost either way.

## SR-AUD-173 — `CharUnicodeInfo` numeric queries (#2335 `done`, #2336 `blocked`)

**Live state.** `GetDecimalDigitValue` accepts U+0030–U+0039 only. `GetDigitValue` adds
U+00B9/U+00B2/U+00B3. `GetNumericValue` adds to those U+00BC/U+00BD/U+00BE. **Sixteen code
points in total**; everything else returns `-1`/`-1.0`. Reproduced from the source, matching
the audit's probe: U+0665 → -1 (.NET 5), U+216B → -1.0 (.NET 12), U+2153 → -1.0 (.NET 0.333…).

**Consumer surface.** `System::Char::GetNumericValue` forwards to `CharUnicodeInfo::
GetNumericValue` and inherits the reduction exactly (`Char.hpp:410-411`). `Char::IsNumber`
does **not** — it routes through `GetUnicodeCategory`, which is SR-AUD-174's surface and was
repaired for locale dependence and surrogates by #2316. The finding names both; only the
first is this finding's.

**Clause implemented (#2335, `done`), documentation only.** The finding's own words are "This
is not documented as a limited character set", and that clause needs no data. The class note
gains a numeric-reduction paragraph naming the sixteen code points and the three measured
divergences, and all six method doc-comments are corrected: `@return -1 if @p ch is not a
decimal digit` was false — the value also means *is* a decimal digit that this port does not
know — and now says so. This is the same repair #2316 applied to the category method's
doc-comment, applied to its siblings; nothing else about #2316 is reopened.

**Clause blocked (#2336).** A faithful implementation needs the Unicode
`Numeric_Type`/`Numeric_Value` data for the whole code space. No Unicode character database
exists in this repository (verified: no `UnicodeData.txt`, no `DerivedNumericValues.txt`, no
generated table, no generator), `/rv` is absent, and no ICU dependency exists. The governing
decision — data source, attribution, stated version, update policy — is **already before the
user as Approval F / #2018**; #2336 is a consumer of it, exactly as #2315 is for the category
table, and asks nothing new. The tables are different (numeric fields versus general
category), so this is a shared gate, not a shared implementation. No partial table is
hand-authored.

## SR-AUD-182 — `StringNormalizationExtensions` (#2337 `done`, #2338 `blocked`)

**Live state.** `IsNormalized(str, form)` returns `true` unconditionally and
`Normalize(str, form)` returns its argument; the two default overloads forward to them. The
header already discloses the ASCII-only stub — the finding acknowledges that and says it is
not enough, because a positive answer from `IsNormalized` is what a guard acts on.

**Consumer surface: none in production.** The only in-repository uses are five tests in
`StringTests.cpp`; there is no `String::Normalize`, and no module calls the type.

**Clause implemented (#2337, `done`), test only.** Every one of those five tests uses ASCII
or the empty string, so **all of them pass identically before and after a real normalization
implementation** — the divergence had no pin at all. That is the same gap #2022 found and
closed for SR-AUD-294, and the same remedy applies: +4 gated-behaviour pins recording the
current answers for the decomposed sequence the audit measured (`65 CC 81`, reported
normalized for Form C and returned unchanged rather than composed to `C3 A9`), the composed
form under Form D, the compatibility ligature U+FB01 under Form KC/KD, and the finding's
separate observation that an **undefined** `NormalizationForm` value is accepted as a normal
success where .NET reports an argument error. Each pin must be inverted by the repair, which
is the point. No production file touched.

**Clause blocked (#2338), and its dependency is bigger than the other three.** Normalization
needs the canonical and compatibility decomposition mappings, the canonical combining classes,
the composition exclusions and the quick-check properties — **and** a UAX #15 implementation
to use them; it is the only one of the four Unicode-blocked tickets that is not satisfied by a
table lookup. The **data-source decision** is nonetheless the same one, Approval F / #2018, so
#2338 records it as the gate and does not raise a second approval. Deliberately **not** done:
throwing `PlatformNotSupportedException` for non-ASCII input. That would be a runtime
behaviour break on currently accepted input, chosen without approval, in a type whose
disclosure already says the opposite.

---

## Addendum — two findings the umbrella was carrying, found when it closed

Re-running the strict ownership analysis with **#2317 closed** turned up two open Core
findings whose only remaining references were `done` tickets. Both are real gaps in the
previous inventory, not artefacts of closing the umbrella, and both now have their own ticket.

**SR-AUD-114 (`Attribute.hpp`) → #2339, `needs_user`.** #2317's description listed it among
the findings it was claiming, but the previous batch's review pool did not include it and no
ticket was ever created; its only other reference was #1766, the discovering audit. Reviewed
here from scratch. It is the same shape as SR-AUD-068 — a public constructible base whose
default `Equals`/`GetHashCode` are identity-based where .NET's are reflection-driven and
fieldwise — and **materially worse**: 46 types in this repository derive from `Attribute` and
**not one overrides `Equals`**, against `ValueType`'s zero production derivations. The
fieldwise half is reflection (permanent deviation, and impossible in C++ from a base class);
the rest is a public source break that also retires nine tests.

**SR-AUD-176 (`Numerics/BFloat16.hpp`) → #2340, `todo`, deliberately not started.** #2261
rejected it from its unit in so many words — "left confirmed and unclaimed" — and #2317's only
mention of it is the *methodological example* "`SR-AUD-175/176`", which is exactly the
false-ownership signal the strict method is written to exclude. It was therefore never owned,
and the earlier 33/33 reading counted it through a sentence about parsing. #2340 records the
finding, states that the work is additive (no approval boundary) but unbounded as one ticket,
and fixes a six-way decomposition that must become bounded tickets before any of it starts.

**One bookkeeping correction, no state change.** #2323 was the only open Core owner whose
*title* did not name its finding, so a title-scoped scan read SR-AUD-092 as owned only through
incidental cross-mentions. Its title now carries `(SR-AUD-092)`, the convention its four
sibling review tickets already follow.

### Strict ownership, after this batch

All 33 open `modules/core` findings have an **open ticket that names them in its title**:
25 `needs_user`, 3 `todo`/deferred (SR-AUD-092/#2323, SR-AUD-176/#2340, SR-AUD-177/#2268),
5 `blocked` (SR-AUD-050/#2228, SR-AUD-088/#2059, SR-AUD-173/#2336, SR-AUD-174/#2315,
SR-AUD-182/#2338). **Zero are implementation-ready**: every remaining open Core finding now
needs either a user decision, external reference data, or a decomposition that has not been
authorised. The method is unchanged from #2317's — compressed runs such as `SR-AUD-173/174`
are expanded, #1766 is excluded as the audit that *discovered* every finding rather than a
ticket that took responsibility for one, and a mention inside a "not a family with X" clause
or a sequencing note is not ownership.
