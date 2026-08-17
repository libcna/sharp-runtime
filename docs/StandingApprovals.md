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

1. **No vtable, mangled-symbol, signature or `noexcept` change** is involved. Adding a virtual
   member, changing a base class, or altering an exception specification is **outside** this
   approval and still needs a per-action ask.
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

## 4d. SA-7 — the `NotifyFilters` → inotify mapping (ticket #2346)

> Answer the five priced questions of `docs/SystemIONamespaceReviewPlan.md` §21.10 as
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
