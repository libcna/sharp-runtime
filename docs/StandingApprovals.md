<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Standing approvals and measured environment facts — post-audit work

This document is the durable record of the **standing approvals** the user granted on
**2026-08-17**, and of the **environment facts measured on the same date** that invalidate the
stated gate of a large number of open tickets. It grants authority; it remediates nothing by
itself. Every approval below is quoted in the form it was granted, with its conditions and with
what it explicitly does **not** cover.

Before this document, every layout change and every public source break needed a fresh
per-action approval, and 78 tickets recorded "`/rv` absent" or "downstream consumers may not be
inspected" as an unresolvable gate. Both of those gates are now settled — the first by decision,
the second by measurement.

---

## 1. SA-1 — working branch and push target

> Commit and push directly to **`next`**.

Granted 2026-08-17, replacing the `feature/work` default of `CLAUDE.md` rule 3 **for post-audit
work in this worktree only**.

**Conditions and limits.**

- `next` had **no upstream** when this was granted; the first push is `git push -u origin next`.
- CLAUDE.md rule 13 still applies unchanged: **push immediately after every commit**, never end a
  turn with the local branch ahead of its upstream.
- CLAUDE.md rules 3 and 9 are **not** relaxed for anything else: `develop`, `master` and tags
  still need explicit per-action user approval, and this approval never extends to them.

---

## 2. SA-2 — public source breaks are permitted, and must be measured against the real consumers

> A public source break may land, with the machinery below, and its impact on the local `cna` and
> `mobile-eggbert` checkouts must be **measured and reported**.

Granted 2026-08-17. This settles the sentence that 59 open tickets record as their gate — *"zero
first-party production consumers does NOT license the break: downstream consumers exist and were
not inspected"*. They exist **and they are inspectable** (§5.2).

**A break may land only when all five hold.**

1. It is recorded in a `docs/Migration-*.md` note.
2. Every outlawed spelling has a `test/consumer/*_negative.cpp` site, per the negative-fixture
   contract in `CLAUDE.md` and `docs/NegativeConsumerFixtureValidation.md` — never a whole-file
   "does not compile" assertion.
3. A downstream migration ticket is filed in the shape of **#1773**.
4. The **full gate** runs: `scripts/run_component_tests.sh build`, with no test-count regression.
5. The impact on `/rv/data/development/github.com/openeggbert/cna` and
   `/rv/data/development/github.com/openeggbert/mobile-eggbert` is **measured** — the affected
   spelling is searched for in both trees — and the finding is reported in the ticket record,
   whether it is zero sites or many.

**What this approval does not grant.** It does not authorise editing `cna` or `mobile-eggbert`.
Those repositories are changed **only on an explicit per-action instruction**; a measured break in
them is reported, not silently repaired. It does not authorise a vtable or ABI break — that is
SA-3's boundary, and SA-3 excludes it too.

---

## 3. SA-3 — standing approval for object-layout growth in public types

> A private data member may be added to, or removed from, a public type **without a fresh ask**,
> under the four conditions below. Anything touching a vtable or a base class still asks.

Granted 2026-08-17, replacing the per-action regime that #1788 and #1789 established.

**Conditions — all four are mandatory.**

1. **No mangled-symbol, signature or `noexcept` change** is involved. Altering an exception
   specification is **outside** this approval and still needs a per-action ask.
   **AMENDED 2026-08-20 BY SA-15.3**: adding a virtual member or changing a base class used to be
   excluded here and no longer is — it lands under SA-15.3's five conditions, one of which
   (enumerating every `catch` clause whose meaning changes) exists precisely because a
   reparenting is invisible to a layout pin.
2. The **before/after `sizeof` and `alignof` are measured and pinned by a layout test** in the
   same change. A claim without a pin does not satisfy this condition.
3. A `docs/Migration-*.md` note records the change and the **full-consumer-rebuild requirement**.
   A `sizeof` change across a stale-header boundary is an ODR violation with no diagnostic; the
   library ships as a **static library built from source**, so this forces a rebuild rather than
   breaking a distributed binary — a real cost, and not the same cost as a shared-library ABI
   break.
4. The **full gate** runs, with no test-count regression.

**Immediately authorised by SA-3.** **Approval IO-1**
(`docs/SystemIONamespaceReviewPlan.md` §21.8, ticket **#2098**) satisfies all four conditions:
one private `bool` in each of `StringReader`, `StringWriter`, `StreamReader` and `StreamWriter` —
**not** in `TextReader`/`TextWriter` — growing `sizeof(StringWriter)` **384 → 392** with the other
three unchanged, no vtable/mangled-symbol/signature/`noexcept` change, and with the "before" half
of the pin **already landed by #2108**. Option (b) of that plan, a flag in the two base classes,
is **not** authorised: it changes six types' layout including both bases, which condition 1
excludes.

**Explicitly still declined**, and not to be re-proposed under SA-3: **#1888, #1889, #1896**.

---

## 4. SA-4 — Unicode table source, version and update policy

> Derive the tables from **.NET's own generated data in `/rv`**, at **UCD 16.0**; cross-check
> against the local Perl and Python corpora and document every disagreement.

Granted 2026-08-17. The previous recommendation to defer rested on the premise that no data
source was available, which §5.3 shows to be false.

**Source of record.**
`/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Globalization/CharUnicodeInfoData.cs`
— the table .NET itself consumes, MIT-licensed, already covered by this project's .NET
attribution header. `/rv/tmp/runtime` declares `<UnicodeUcdVersion>16.0</UnicodeUcdVersion>`
(`GenUnicodeProp.csproj`), so the derived table is at **Unicode 16.0** and parity with .NET is
**derived, not declared**.

**Cross-check corpora**, offline, in this container: Perl 5.40.1 `unicore` at Unicode **15.0.0**
(`/usr/share/perl/5.40.1/unicore/`) and Python 3.13.5 `unicodedata` at Unicode **15.1.0**.
Disagreements between the source of record and either corpus are expected to confine themselves
to codepoints added or reclassified in Unicode 15.1 and 16.0, and **every disagreement must be
listed in the design record** rather than averaged away.

**Policy.** The generator and the generated table are both committed, with UCD attribution and the
Unicode licence notice. The version is named in the generated header and in every test file. The
version is **pinned until an explicit bump ticket**; no build step downloads anything, and
`www.unicode.org` is not reachable from this container anyway.

**Unlocks, in this order:** #2315 (general category) → #2336 (`Numeric_Type`/`Numeric_Value`) →
#2018 (`Rune` category, simple case mapping, the U+FEFF divergence) → #2338 (which additionally
needs a UAX #15 algorithm, not only a table).

---

## 4b. SA-5 — aligning to the reference is ordinary work, not an approval request

> When the reference tree says unambiguously what .NET does, align the port — **including where
> a call that succeeds today starts throwing, or emitted text changes.** Do not ask per ticket.

Granted 2026-08-17, and it is the single largest unblocking decision on this page. Before `/rv`
was available, such a change would have been a **guess** dressed as parity, which is why the
review deferred so many of them; with the source in hand it is a **derivation**.

**Every change under SA-5 must carry all four.**

1. **A citation**, in the code comment and in the ticket: file and line in
   `/rv/tmp/runtime/src/libraries/`, quoted closely enough that a reader can check it.
2. **A migration note** whenever accepted input, emitted text or exception identity moves.
3. **The full gate**, with no test-count regression.
4. **The measured downstream report** of SA-2 condition 5 whenever a public spelling is
   affected.

**Two limits.** SA-5 does not license a **layout** change (that is SA-3's four conditions) or a
**source break** (SA-2's five). And it does not license inventing .NET behaviour: where the
reference is ambiguous, platform-split, or simply absent for the case in hand, the question is
still deferred or asked — SA-5 covers derivation, never guesswork. Note the recurring hazard
recorded in §5.1: this is a **.NET 11** snapshot, so a behaviour that could plausibly have moved
since .NET 8 must be reported as .NET 11's rather than as timeless parity.

**Why it was granted, in one measurement.** Of the first seven tickets settled against the
restored reference, **five had pinned an answer the reference contradicts** — #1963, #2242
(twice over: the question asked *and* a platform split the ticket never suspected), #2192 (three
ways), #2252 and #2260's second half. Only #2234 and #2260's first half confirmed what the port
already did. Deferring in the absence of evidence was correct; leaving those pins standing once
the evidence exists would not be.

---

## 4c. SA-6 — a test that encodes an environment version is a defect in the test

> Where the gate fails because a test hard-codes something the environment now reports
> differently, and the port is measurably right, **fix the test** — derive the expectation from
> the same source the production code reads.

Granted 2026-08-17 for the two `TimeZoneInfo` `BaseUtcOffset` cases (ticket **#2351**), and as a
general rule for the same shape. The obligation is the same one that applies to any test change:
the repaired test must still fail for the defect it was written to catch, demonstrated by
**mutation**, and never merely by being made weaker.

**This is how the complete independent gate first became green** — 17,150 run, 0 failed, 38
executables, 2026-08-17. Every earlier checkpoint in `CLAUDE.md` rule 2 ends with *"the gate is
not green"*.

---

## 4e. SA-8 — public representation: match .NET, and migrate the sites

> Where this port publishes a mutable or public representation and .NET's is private, readonly or
> absent, **match .NET**. Break the source and migrate the first-party sites.

Granted 2026-08-18, answering the nine-ticket family **#2322, #2324, #2325, #2326, #2327, #2328,
#2330, #2332, #2339**.

**The user chose against the recommendation, and did so knowing the price.** The alternative
offered was to split the family by hazard — repair `SequencePosition` and `Attribute`, pin
`Tuple::Item1` as cosmetic — and the objection stated at the time was that `t.Item1` becomes
`t.getItem1Property()` under CLAUDE.md rule 5, permanently, in a CNA-facing API. The answer was
*always match .NET*. That is the decision; it is not to be re-litigated ticket by ticket.

**What it authorises**, over and above SA-2, whose five conditions still all apply to each landing:

* making a public data member private and adding the rule-5 accessor pair;
* migrating first-party read and write sites to the accessor spelling — 75 of them in `Tuple`
  alone, all in this repository's own tests;
* making a public default constructor protected, or a concrete class abstract, where .NET's is;
* replacing an identity `Equals`/`GetHashCode` with .NET's field-wise definition.

**What it does not authorise.** A vtable or base-class change still asks (SA-3's exclusion is
unchanged). And SA-8 is not a licence to invent: where .NET's own shape is unclear the answer is
still derived from the reference or deferred, never guessed — SA-5's limit applies here too.

Each landing still carries its own migration note, per-spelling negative consumer fixture,
downstream ticket, full gate and measured consumer impact.

---

## 4f. SA-9 — out-of-scope types wear .NET's shape and throw

> For a type that exists only because .NET has one, and whose content is permanently out of scope
> (reflection, serialization, remoting, interop): **the public shape matches .NET, and the bodies
> throw.**

Granted 2026-08-18, answering **#2276, #2281, #2291, #2295, #2297, #2298, #2334**.

The rule resolves a third state these types are in today, which is the worst of the three: the
surface is neither .NET's nor internal, so it is a project-owned API wearing a .NET name.
`MarshalByRefObject` is directly constructible where .NET is abstract; `LocalDataStoreSlot` has a
public default constructor .NET makes internal, and one `std::any` shared by every thread, which
is a data race.

**The shape:**

* abstract where .NET is abstract, protected constructor where .NET's is protected, internal
  constructor rendered unreachable where .NET's is internal;
* members .NET keeps are **present**, so a caller receives a runtime diagnostic
  (`PlatformNotSupportedException`, or `NotImplementedException` where CLAUDE.md's parity section
  already names one) instead of a compile error;
* members .NET does **not** have are removed.

**One consequence was priced and accepted at grant time:** `LocalDataStoreSlot` reaches its .NET
shape only behind a new `Thread::AllocateDataSlot` / `AllocateNamedDataSlot` / `GetData` /
`SetData` / `FreeNamedDataSlot` surface, which this repository does not have. That is
substantial new public API and it is authorised.

CLAUDE.md's parity section already required a stub to throw "with a comment explaining why —
never silently return a wrong value". SA-9 adds the half that was missing: the *shape* is .NET's
too, not this project's invention.

---

## 4g. SA-10 — SA-2 covers a public signature change

> A change to a public **signature** — return type, `noexcept`, nullability, `[[deprecated]]` — is
> a public source break, and lands under SA-2's five conditions like any other.

Confirmed 2026-08-18. This is a clarification, not a new grant: SA-2 already said "a public source
break may land with a migration note, a per-spelling negative consumer fixture, a #1773-shaped
downstream ticket, the full gate, and a measured impact report against the local `cna` and
`mobile-eggbert` checkouts". Nine tickets were nonetheless recorded as `blocked` on the theory
that SA-3's narrower object-layout wording was the only relevant approval. It is not.

Unblocks **#2170, #2172, #2185, #2215, #2246, #2250, #2269, #2289, #2299**.

**Unchanged:** a vtable or base-class change still asks per action, and #1888, #1889 and #1896
stay declined.

**One correction, recorded rather than quietly fixed.** SA-10's grant bulk-unblocked nine tickets,
and **#2170 was among them in error**: it adds `IEqualityComparer<T>` as a **base class**, which
introduces a second vptr — measured, `sizeof` 8 → 16 with the subobject at offset 8. That is a
base-class change, which the paragraph above excludes in terms, so #2170 **returned to
`needs_user`** on the same day. Two further notes named SA-10 where SA-3 (#2185, a private data
member) and SA-5 (#2269, a behaviour narrowing) are the right approvals; those tickets stay
unblocked and only the citation was wrong.

---

## 4h. SA-11 — five decisions taken on 2026-08-18

Not standing approvals but recorded here because each settles a ticket that repository evidence
could not, and each must survive a context reset.

| Ticket | Question | Decision |
|---|---|---|
| **#2109 / #2131** | mint CCF-021 and CCF-022? | **mint both** — a cause spanning more than one namespace cannot be owned by any single namespace review, and "the numbering is closed" was only ever scoped to namespace-local causes |
| **#2320** | should POSIX `GetFolderPath` follow XDG? | **option B** — honour `XDG_CONFIG_HOME` / `XDG_DATA_HOME` when set **and absolute**; a relative value is ignored per the spec. Full XDG declined: part of it has no .NET mapping, so it would cross from parity into invention |
| **#2238** | may `Lazy<T>` `PublicationOnly` match .NET? | **yes** — factory outside the mutex, mutex only across publication, first writer wins |
| **#2115** | two inert `JsonDocumentOptions` flags | **make both real** |
| **#2138** | `TcpClient`/`UdpClient` are AF_INET only | **reject an IPv6 endpoint loudly now**; full dual-stack becomes its own ticket |

**Two of these have consequences that were stated before the decision and accepted with it, and
must not be quietly softened later.**

*#2238 reverses a documented deviation.* This port's `PublicationOnly` has serialised the factory
since the type was ported, and the class doc-comment says so. After the repair a factory may again
run **concurrently on several threads** — which is what `PublicationOnly` means in .NET. The
doc-comment is to be rewritten rather than amended, and the migration note must say plainly that a
factory previously guaranteed to run once may now run several times.

*#2115 was chosen over a cheaper honest option.* `nlohmann/json` supports neither trailing commas
nor duplicate-key detection natively, so the two flags are not equally cheap. The alternative
offered was to make `Validate()` throw `NotSupportedException` for whichever could not be
implemented. The answer was to implement both, because a switch that does nothing is worse than a
switch that does not exist.

**#2320's second clause is not a user question after all.** Whether a getter may verify or create
directories is answered by .NET itself — `SpecialFolderOption.Create` / `DoNotVerify` — so it is
derivable under SA-5 and will be taken from the reference rather than asked.

---

## 4i. SA-12 — how this port represents .NET's `internal` members (ticket #2390)

Granted 2026-08-19. **A general rule, not a decision about one type**, and it applies to every
ported type with `internal` constructors or members.

C++ has no `internal`, and the mechanical translations are not equivalent to it, so the rule is
**conditional on whether this port has a real creator**:

| Situation | Representation |
|---|---|
| This port **has** a creator — some type here really does construct it | make the members **`private`** and **`friend`** that creator. This is the shape #2298 already used for `LocalDataStoreSlot` with `Thread`. |
| This port has **no** creator — the window in which .NET's runtime would construct it does not exist here | keep the members **public** and **record the accessibility divergence in the header**. |

**Why the second row is not a cop-out.** Two mechanical alternatives were offered and declined,
each for a stated reason:

* *`private` with no friend* makes the type impossible to instantiate at all. That is arguably a
  .NET user's exact position, but it leaves dead code and costs the tests that verify the type's
  fixed message and HResult — the only observable content such a type has.
* *`private` + a friend that never constructs it* is a **dead friend declaration**: it looks
  faithful and grants access to a class that will never use it.

The divergence in the second row is in **accessibility only**. The parameter lists, message,
HResult and `final`-ness still match .NET exactly, so a caller who writes what .NET allows gets
what .NET gives.

**Worked example — `System::Threading::ThreadStartException` (#1958 / SR-AUD-196, #2390).** .NET
makes both constructors `internal`; the runtime throws it when a managed thread fails *after* the
OS thread starts but *before* user code runs. That window does not exist in this port —
`std::thread` either constructs or throws `std::system_error` — so no code here can ever throw it.
Second row: the constructors stay public and the header says so.

---

## 4j. SA-13 — eleven decisions taken on 2026-08-19

Recorded here for the same reason as SA-11: each settles a ticket that repository evidence could
not, and each must survive a context reset. Ordered by ticket number.

| Ticket | Question | Decision |
|---|---|---|
| **#1896** | may `JsonNode` and `XContainer` grow so the ownership guard stops walking the ancestor chain? | **approved** — the 2026-07-31 refusal is **withdrawn**. The guard must get *faster, not weaker*: it keeps rejecting exactly what it rejects today |
| **#1899** | X15/X17 cannot be made unreachable — what instead? | **leave the contract documented**, as #1898 left it, and close #1899 carrying its own impossibility proof |
| **#2118** | `JsonElement::GetRawText` re-renders instead of returning source text | **declare the limitation and pin it** (the #2202 shape). Retaining source spans was declined: paid at parse time and in memory by every caller, called or not |
| **#2155** | may `Timers::Timer` derive from `System::Object` so `Elapsed` reports a sender? | **approved** — `sizeof` 104 → 112 **and a new vtable**, accepted knowingly |
| **#2170** | may `TotalOrderIeee754Comparer` grow 8 → 16 for `IEqualityComparer<T>`? | **approved** — second vptr, on the measurement that it has zero users outside its own tests |
| **#2185** | is tzdata rule parsing in scope, so `HasSameRules` can distinguish rule sets? | **out of scope** — SR-AUD-228 becomes a **permanent deviation**, "not closable" rather than "not yet closed" |
| **#2199** | XObject notification: layout, and how does removal identify a registration? | **both granted**: `sizeof(XObject)` 16 → 24, and `add_*` returns a **registration token** that `remove_*` takes |
| **#2366** | may `cna`'s two `SetEnvironmentVariable(name, "")` sites be edited? | **yes**, per-action, those two sites only, **no commit** |
| **#2377** | may `cna`'s six GamerServices sites be edited? | **yes**, per-action, that repair only, **no commit**. Reverting #2323 was offered and declined |
| **#2390** | how to represent .NET's `internal`? | **SA-12 above** |
| **#2391** | should `UriBuilder::Equals`/`GetHashCode` delegate to `Uri` as .NET does? | **yes** — #2004's non-throwing guarantee is **withdrawn** |

**Three of these have consequences that were stated before the decision and accepted with it, and
must not be quietly softened later.**

*#2391 withdraws a guarantee this port measured and chose.* After it, `UriBuilder::Equals` and
`GetHashCode` **both throw** for a builder whose rendering does not parse — including
`b.Equals(b)`. The four routes are ordinary setters. #2004's tests are to be **inverted**, not
deleted, and its doc-comment rewritten to say the guarantee was given up for parity.

*#2155 and #1896 were taken against the recommendation on the record.* Both were recommended for
refusal — #2155 because a new vtable on a public type is the most expensive break available, #1896
because it speeds up working code rather than adding a missing feature. Both were granted anyway;
that is the user's call and the reasoning above is preserved so the trade is visible, not so it is
relitigated.

*#2185 closes a finding permanently.* SR-AUD-228 joins the reflection deviations, carrying two
measurements: the finding is **not closable by sampling libc at any granularity**, and the failure
is **one-directional** — this port can only be too permissive, never too strict.

---

## 4k. SA-14 — three decisions taken on 2026-08-20, unblocking the date/time chain (#1940)

Granted 2026-08-20. **#1940 is the root of the remaining date/time chain** — #1942, #1943 and #1945
list it as a dependency and #1944 depends on #1943 — so these three answers unblock **five** tickets.
All three were recommended and all three were granted as recommended.

| # | Question | Decision |
|---|---|---|
| 1 | how should the provider reach the parser, given that `Core.Base -> Globalization` is a cycle? | **C, then A**: move `DateTimeFormatInfo` into `Core.Base`, then add the provider overloads on top |
| 2 | may the culture-concurrency defect be repaired separately and first? | **yes** — its own ticket, ahead of #1940, using the **two-property** model |
| 3 | what should an unrecognised culture *name* do? | **throw `CultureNotFoundException` from BOTH doors** — the constructor and `GetCultureInfo` |

**Why C is cheap, and it is measured rather than estimated.**
`System/Globalization/DateTimeFormatInfo.hpp` is **header-only** (no `.cpp`), does **not name
`CultureInfo`**, and its only non-`Core.Base` include is the enum header `CalendarWeekRule.hpp`. It
has **four** includers, all inside Globalization, which already depends on `Core.Base`. And
`validate_module_boundaries.py` assigns ownership by **logical path uniqueness**, not directory
prefix, so the header keeps the path `System/Globalization/DateTimeFormatInfo.hpp` and **not one
include line anywhere changes**. Two files move; the graph stays **41 / 94**.

The alternative the ticket's own wording implied — shape **B**, a new component holding
`DateTime`/`DateOnly`/`TimeOnly` — was measured at **34 including files across eight modules**
(`core`, `globalization`, `io`, `net`, `threading`, `timers`, `time-zone`, `xml`). *"An explicit
component plus an ABI transition"* is true of B and measurably false of C.

**Two consequences were stated before the decision and accepted with it.**

*Decision 2 adds public surface, and the naive repair is wrong.* This port's `currentCulture_` and
`currentUICulture_` are **static members**, so they are process-wide: a set on one thread changes
what every other thread reads, and a concurrent get/set is an unsynchronised write to a non-atomic
object — while the type's own doc-comment says *"the current **thread's** culture"*. Simply making
them `thread_local` would fix the race and **silently remove the process-wide setting the port
accidentally has today, with no replacement**. .NET's chain is
`s_currentThreadCulture ?? s_DefaultThreadCurrentCulture ?? s_userDefaultCulture`
(`CultureInfo.cs:358-366`), and `DefaultThreadCurrentCulture` is a real public static property
(`:407-413`). The faithful repair is therefore a **two-property model**, and that new surface is
part of what was granted.

*Decision 3 is a narrowing: code that compiles and runs today will start throwing.* It needs a
migration note and a measured downstream report, like any SA-5 narrowing.

**Two premises on the #1940 record were re-measured on 2026-08-20 and found WRONG**; the decisions
above rest on the corrected versions.

- The note said *"this port has no `CultureNotFoundException` anywhere in `modules/`"*. **It does** —
  `modules/globalization/include/System/Globalization/CultureNotFoundException.hpp`, deriving from
  `ArgumentException` — and it is **already thrown**, but only from the **LCID** path
  (`ValidateLcidStub`, five specific numeric values), never from a name. So decision 3 does not
  invent an exception type; it makes **two doors of one type consistent**, which is #2393's shape.
- The note described the unknown-name behaviour as *"resolves to InvariantCulture"*. Measured, it is
  worse: `CultureInfo("xx-YY")` **succeeds**, `getNameProperty()` returns `"xx-YY"`, and the format
  objects are populated from the **invariant** culture — so the object **claims to be `xx-YY` and
  behaves as invariant**.

---

## 4l. SA-15 — three decisions taken on 2026-08-20

Granted 2026-08-20, after SA-14's work exposed each of them. All three were recommended and all
three were granted as recommended.

| # | Question | Decision |
|---|---|---|
| 1 | how should `DateTime` reach a timezone, given the same cycle #1940 had? | **an abstraction in `Core.Base`** that `TimeZoneInfo` implements — shape **A**, not the move |
| 2 | what counts as an *unrecognised* culture name in a port with no culture database? | **a syntactic BCP-47 check** — `""`, `"und"` and any well-formed name are accepted; malformed ones throw |
| 3 | may a **vtable or base-class** change land under a standing approval? | **yes — SA-3 is extended to cover them**, under the conditions below |

### SA-15.1 — the timezone abstraction, and the caveat that came with the grant

**A timezone provider already exists and is good**: `System::TimeZoneInfo` is 968 header lines plus
328 of implementation, has **190 passing tests**, reads real tzdata, and already offers
`ConvertTimeToUtc(DateTime)`. The obstacle was never capability; it is that `TimeZone` declares
`PUBLIC_DEPENDENCIES Core.Base`, so `DateTime` naming `TimeZoneInfo` would be **#1940's cycle
again**.

Moving it was measured and rejected as the more expensive shape: unlike `DateTimeFormatInfo` it is
**not header-only**, it carries two exception types and a 270-line private POSIX support header, it
has four includers outside its own module (one of them a *public* header,
`threading/System/TimeProvider.hpp`), and it would put **tzdata reading under every consumer of
`Core.Base`**.

**The caveat was stated before the decision and accepted with it.** .NET's `ToLocalTime()` takes
**no argument**, so an abstraction in `Core.Base` has nowhere to get its source from. The two ways
out are a **registration hook** — hidden global state with a static-initialisation-order dependency
— or an **overload that takes the source explicitly**, which is a deviation from .NET's signature.
The second is the one consistent with what this repository has already chosen twice: #1940's
`GetInstance(nullptr)` resolves to the invariant info and tells the caller to *pass the culture*,
and the port has refused service-locator shapes before. **Whichever is implemented must be recorded
as a deviation rather than presented as parity.**

### SA-15.2 — the culture-name boundary

**Accepted**: `""`, `"und"`, and any well-formed BCP-47 name (`xx`, `xx-YY`, `xx-Latn-YY`), which
continue to behave as the invariant culture. **Rejected**: anything malformed — `"process-default"`,
`"de_DE"`, `"123"`.

**This is deliberately NOT .NET's invariant-globalization behaviour, and that must be said plainly
in the header rather than implied.** Measured: `CultureData.cs:660-675` with
`GlobalizationMode.cs:19` shows `PredefinedCulturesOnly` defaulting to `GlobalizationMode.Invariant`,
so .NET in this port's own mode accepts **only** `""` and `"und"` and throws for **every** other
name, `"de-DE"` included. The decision takes the *shape* of .NET-without-a-database instead,
because it catches the accept-and-ignore cases that motivated SA-14 decision 3 without making the
type unable to represent any named culture at all.

Measured first-party cost: **zero** real sites (only the two existing `"de-DE"`/`"ja-JP"` test
constructions, which stay legal). Downstream: **zero**.

### SA-15.3 — SA-3 now covers vtable and base-class changes

SA-3's first condition previously read *"no vtable, mangled-symbol, signature or `noexcept` change
is involved… Adding a virtual member, changing a base class… is **outside** this approval"*. That
exclusion is **lifted**, under **all** of the following:

1. the before/after `sizeof` **and** the vtable change are **measured and pinned** by a layout test;
2. a `docs/Migration-*.md` note records the **full-consumer-rebuild** requirement;
3. the **measured downstream report** of SA-2 condition 5 is produced against both consumers;
4. **every `catch` clause whose meaning changes is enumerated explicitly** — a reparenting silently
   changes which handlers fire, and that is the part a `sizeof` pin cannot see;
5. the **full gate** runs with no test-count regression.

**Immediately unblocked**: **#1980 G-3** (reparenting `AmbiguousImplementationException` and
introducing `OSPlatformAttribute` as a base) and **#1997 A-4**. Condition 4 exists because of G-3
specifically: it breaks `catch (const SystemException&)`, which no layout assertion would reveal.

**Still explicitly declined**, unchanged: **#1888, #1889, #1896** are not re-proposed under this.

---

## 4d. SA-7 — the `NotifyFilters` → inotify mapping (ticket #2346)

> Answer the five priced questions of `docs/SystemIONamespaceReviewPlan.md` §21.11 as
> **1a, 2a, 3a, 4c, 5b**.

Granted 2026-08-17. This one is **not** derivable from the reference and never will be: .NET's
`NotifyFilters` vocabulary describes Win32 `ReadDirectoryChangesW` notifications, and inotify's
event set is not a relabelling of it. The decision is therefore the user's, and it is:

| # | Question | Answer |
|---|---|---|
| 1 | `IN_MODIFY` serves… | **(a)** `Size` + `LastWrite` |
| 2 | `IN_ATTRIB` serves… | **(a)** all six attribute-class filters |
| 3 | `CreationTime`, for which no inotify event exists | **(a)** approximate through the content class |
| 4 | `LastAccess`, currently unserved | **(c)** add `IN_ACCESS` **only** when `LastAccess` is named |
| 5 | `FileName` vs `DirectoryName` | **(b)** gate `Created`/`Deleted`/`Renamed` on `IN_ISDIR` |

The shape of the answer is *permissive where Linux genuinely cannot discriminate, discriminating
where it can*. Over-notification is recoverable by a caller; silence is not — which is what
decides 1, 2 and 3. Where the information does exist, the two filters are meant to differ, which
decides 5. And 4 avoids making every read wake every content watcher while still letting a
caller who names `LastAccess` receive events for it.

---

## 4m. SA-16 — three decisions taken on 2026-08-20, closing the date/time zone chain

Three decisions, all taken as recommended, which together close the remaining blockers on
**#1942**, **#1943** and the `RoundtripKind` limitation **#1945** declared.

### SA-16.1 — `DateTime`'s style overloads take the zone as a parameter

The kind-affecting `DateTimeStyles` members **convert**: `AdjustToUniversal` must produce UTC and
`AssumeLocal` must interpret an unqualified value as local. .NET reaches `TimeZoneInfo.Local`
internally; `Core.Base` cannot name a time zone at all.

**Granted: the style-taking overloads take a `const System::ILocalTimeZone*`.** This is exactly the
precedent **#1941 phase 2** set and that **SA-15.1** already accepted once as a recorded deviation —
`ToLocalTime(zone)` / `ToUniversalTime(zone)` — so the port is consistent with itself rather than
answering the same question two ways.

Two alternatives were declined on the record. Accepting only the *stamping* styles and rejecting
`AdjustToUniversal` keeps .NET's signature but makes a legal .NET style illegal here — an
asymmetry .NET does not have. A registration hook keeps the signature too, at the price of hidden
global state and a static-initialisation-order dependency, which **#1940 already refused once**.

**The deviation is one parameter and must be stated in the header rather than implied.**

### SA-16.2 — `DateTimeOffset::ParseExact` gets an offset token, and the zone for the default

**An offset is not a time zone.** A format carrying an explicit offset needs no zone database at
all — the offset is read from the input and stored, and `DateTimeOffset` is a `DateTime` plus a
`TimeSpan`. Only the **no-offset** case needs a zone, because .NET's `DateTimeStyles.None` gives
such a result the **local** offset.

**Granted: add the offset token to the exact grammar, admitted for the types that can carry one,
and take the zone as a parameter for the no-offset default** — consistent with SA-16.1 rather than
a second answer to the same question. Requiring an explicit offset instead was declined: it would
be a narrowing .NET does not have, and it would leave
`ParseExact("2024-06-15", "yyyy-MM-dd")` failing where .NET succeeds.

### SA-16.3 — `RoundtripKind` is made real, in both directions

**#1945 measured that a kind cannot cross a string in this port**: `DateTime::Parse` sets no kind
from a `Z` or an offset — #1929 records that it parses one and **discards** it — and
`XmlConvert::ToString` emits no marker where .NET's `XsdDateTime` does. So `RoundtripKind` and
`Unspecified` are observationally identical, which #1945 declared and pinned.

**Granted: close it in BOTH directions.** The parse side sets the kind from a zone token, and
`XmlConvert::ToString` emits the marker as `XsdDateTime` does.

**The output change is accepted knowingly and is the reason this needed asking**: today
`XmlConvert::ToString(value)` renders `2024-06-15 12:00:00`, and it becomes the ISO form carrying a
kind marker. That is a **behaviour change on a public member**, it lands under SA-5 with a migration
note, and #1945's declaration pin is expected to fail and be inverted rather than deleted.

Closing only the parse half was declined for a stated reason: with nothing writing a marker there
is nothing to read, so it would close half a round trip and leave the pin standing.

### SA-16.4 — the general `DateTime::Parse` is left alone

SA-16.3's *"the parse side sets the kind from a zone token"* reaches **`ParseExact` only**.
`DateTime::Parse(s)` and `TryParse(s, result)` keep #1929's behaviour: they parse an offset and
**discard** it, and return an `Unspecified` value.

**Granted deliberately, knowing it leaves a divergence**: .NET's `Parse` converts a zone-qualified
input to local time under the default styles. Those members have no zone and no style parameter, and
changing a widely used one was declined against the benefit. **The divergence is declared rather
than repaired**, and a caller who wants the .NET behaviour uses `ParseExact` with a style and a zone.

### SA-16.5 — `XmlConvert::ToString` adopts the **full** `XsdDateTime` form

Not merely a kind marker appended to today's rendering: **the `T` separator too**. Today's
`2024-06-15 12:00:00` becomes `2024-06-15T12:00:00Z`.

**Two changes rather than one, and that is the point of asking.** Appending only the marker would
repair the round trip and still leave the document wrong, because an XSD `dateTime` literal requires
the `T`. Both overloads are affected, including the one that takes no mode.

### SA-16.6 — `DateTimeOffset::ParseExact` publishes .NET's zone-less overloads

They exist, and a format with no offset token raises `ArgumentNullException` naming `zone` with a
message saying what to pass — **consistent with #1942** rather than a second answer.

**The cost is accepted and is larger here than for `DateTime`**: there, only a few styles need a
zone; here, **every format without an offset token does**, so the most ordinary call raises unless
the caller supplies one. Requiring the zone in the signature was declined for diverging further from
.NET, and refusing the no-offset format was declined as a narrowing .NET does not have.

---

## 5. Environment facts, measured 2026-08-17

These are measurements of this container, not decisions. They are recorded because a large number
of tickets state the opposite as their gate, and because a future session in a container without
them must be able to tell the difference.

### 5.1 The .NET reference tree is present

`/rv/tmp/runtime` exists — 830 MB, 245 directories under `src/libraries`, no `.git` (a snapshot,
not a clone). `eng/Versions.props` declares `<ProductVersion>11.0.0</ProductVersion>` and
`global.json` pins SDK `11.0.100-preview.5.26227.104`, so it is a **.NET 11 preview** snapshot,
**not** .NET 8.

**Consequence.** The 78 open tickets whose recorded gate is "`/rv` absent" — including all 24
`DEFERRED VERIFICATION` items — are answerable. **With one caveat that must appear in each such
ticket:** a behaviour read out of this snapshot is .NET 11's behaviour. Where .NET 8 and .NET 11
could differ, the ticket must say so rather than claim timeless parity.

### 5.2 Both downstream consumers are present

`/rv/data/development/github.com/openeggbert/cna` and
`/rv/data/development/github.com/openeggbert/mobile-eggbert` are live checkouts, and `cna`'s
`CMakeLists.txt` references sharp-runtime directly. This is what makes SA-2's condition 5
possible, and it retires the "may not be inspected" phrasing wherever a ticket still carries it.

### 5.3 Unicode corpora are present offline

Perl 5.40.1 `unicore` at Unicode 15.0.0, Python 3.13.5 `unicodedata` at Unicode 15.1.0, and
.NET's own generated table at UCD 16.0 (§4). No download is required, and none is possible:
`www.unicode.org` is blocked by the proxy.

### 5.4 This worktree

`sharp-runtimenext` is a **git worktree** of `sharp-runtime`, on branch `next`. Its submodule
`vendor/googletest` had never been checked out and was initialised on 2026-08-17 from the shared
`.git/modules` store — no network clone. Its build tree is `build/` (Debug, tests ON, `ccache`,
**two jobs**), created here because the sibling checkout's `build/` belongs to a different source
directory and cannot be reused by CMake. `CCACHE_BASEDIR=/rv/data/development/github.com/openeggbert`
and `CCACHE_NOHASHDIR=1` are exported for these builds so the sibling's ccache entries are
reachable across the two paths.
