<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `Environment::GetFolderPath` on POSIX (SR-AUD-105) — review record

Ticket **#2319**. Reviewed 2026-08-11. SR-AUD-105 stays **`confirmed`**: nothing is
implemented here, and no clause of it is autonomously implementable.

Reviewed on its own, **not** with SR-AUD-106. The two share `Environment.cpp` and were
excluded together by one sentence of #2239, which is adjacency, not a cause: SR-AUD-106 is a
representation problem in one setter/getter pair, SR-AUD-105 is a platform-policy problem in
a different member. No CCF is minted or extended.

---

## 1. Why it had no owner, and what #2239 actually said

Ticket **#2239** (`done`) reviewed the *compatible* Environment slice — SR-AUD-107 and
SR-AUD-108 — and was explicitly told to "state explicitly why SR-AUD-105 (XDG design) and
SR-AUD-106 (public signature change) are NOT in this slice". It did, in
`docs/CoreEnvironmentCompatibleSlicePlan.md` §1:

> **SR-AUD-105** — Unix special-folder resolution. Four separate contracts in one finding
> (absolute `XDG_CONFIG_HOME`/`XDG_DATA_HOME` honouring, the discarded
> `SpecialFolderOption`, filesystem verification and creation, and a range exception for an
> undefined `SpecialFolder`). That is an **XDG design**, needs a decision about creating
> directories from a getter, and would change resolved paths for existing users. Not started
> here.

That is a correct exclusion, not an oversight — but it left the remainder with **no open
ticket**, which is why #2317 claimed it and why this review exists. The same plan already
warned against the trap this review had to avoid: it records that SR-AUD-105's undefined-enum
clause "is the *same shape* as SR-AUD-064" (repaired by the Lazy family) and that it "is
still excluded, because it cannot be separated from the rest of SR-AUD-105's design."
**§3.4 below shows the shape analogy is even weaker than that** — the two are structurally
different problems, so treating them as one family would have produced a wrong repair.

---

## 2. The finding decomposed

Four independent clauses, all in the POSIX branch of `Environment.cpp:289-316`:

| | Clause | Frozen wording |
|---|---|---|
| **C1** | XDG is ignored | hard-codes `$HOME/.config` and `$HOME/.local/share` where .NET honours absolute `XDG_CONFIG_HOME`/`XDG_DATA_HOME`; probe prints `xdg_config=0`, `xdg_data=0` |
| **C2** | `SpecialFolderOption` is discarded, including undefined values | `invalid_option_throws=0`; ".NET rejects an invalid option" |
| **C3** | No verification, no creation | .NET "returns an empty path for a nonexistent folder with `None`, and creates it for `Create`" |
| **C4** | An undefined `SpecialFolder` returns `""` | "rather than .NET's range exception" |

## 3. Measured against the live tree

`build-probe/2319_probe1_specialfolder.cpp`, linked against the shipped library with `HOME`
pinned to a fixed value.

### 3.1 C1 reproduces exactly

`XDG_CONFIG_HOME=/xdg/cfg`, `XDG_DATA_HOME=/xdg/data` (both absolute): `ApplicationData`
still returns `$HOME/.config` and `LocalApplicationData` still `$HOME/.local/share`.
`honoured=0`. No `XDG_` string exists **anywhere** in the repository.

### 3.2 C2 and C3 reproduce exactly

`None`, `DoNotVerify`, `Create` and an undefined `0x1234` all return the same string and none
throws. Under `Create` the directory is not created; under `None` a path that does not exist
on disk is still returned. The two-argument overload takes an **unnamed** parameter and
forwards to the one-argument one.

### 3.3 C4 reproduces — `0x0001`, `0x0003`, `0x0FFF`, `-1`, `999` all return `""`, no throw

### 3.4 The premise correction: `default:` is doing two different jobs

The finding treats the `default:` label as the undefined-value path. It is not only that.
Measured over every enumerator: the enum declares **47 enumerators / 46 distinct values**
(`Personal` and `MyDocuments` are both `0x0005`), and

> **32 of the 46 defined values also fall through to `default: return ""`.** Only **14** are
> mapped: `Personal`/`MyDocuments`, `UserProfile`, `Desktop`, `DesktopDirectory`, `MyMusic`,
> `MyPictures`, `MyVideos`, `ApplicationData`, `LocalApplicationData`,
> `CommonApplicationData`, `ProgramFiles`, `System`, `Fonts`, `Templates`.

An empty return therefore already means *"defined, but this platform has no such folder"* for
32 values — which is what .NET's Unix implementation also does for folders it does not map.
So C4 cannot be repaired the way SR-AUD-064 was. There the enum had three values and
`default:` was unambiguously the error path; here a repair must **enumerate definedness
separately from mappability**: 46 accepted values, 14 undefined values inside the 0x0000–0x003B
span alone, and everything outside it. That is a table, not a guard, and it is the reason
this clause is priced as work rather than a one-liner.

### 3.5 Consumer surface: none in production

`GetFolderPath` has **no first-party production caller**. Its only in-repository callers are
`Environment::getSystemDirectoryProperty()` (itself called only from a test) and
`EnvironmentTests`. `modules/io-isolated-storage` resolves its own paths and does not use it.
So every clause below is priced for *downstream* consumers (CNA, mobile-eggbert), not for
this repository's own code.

### 3.6 Existing coverage pins the current behaviour on purpose

`EnvironmentTests` asserts only that `UserProfile` and `Desktop` are non-empty, that the three
`SpecialFolderOption` enumerators have their CSIDL values, and — in
`GetFolderPath_WithNone_SameAsWithout` and `GetFolderPath_WithDoNotVerify_SameAsWithout` —
**that the option makes no difference**. The audit report says as much ("explicitly assert the
port's ignored-option behavior"). Any C2/C3 repair must retire those two tests, exactly as
SR-AUD-106's repair must retire `Set_Empty_RemovesVar`.

---

## 4. Disposition — what is decidable here and what is not

**Nothing in SR-AUD-105 is autonomously implementable.** The four clauses split across two
different blockers, and neither is a matter this repository has authority to settle.

### 4.1 C1 and C3 are policy, not defects with a known answer — **#2320, `needs_user`**

* **C1** is not blocked on reference text: the frozen finding itself states the rule
  (honour `XDG_CONFIG_HOME`/`XDG_DATA_HOME` **when absolute**). It is blocked on **policy**,
  for two reasons. (i) Honouring XDG *changes where a shipped game reads and writes user
  data* on any host that sets those variables — common on Linux desktops — and the port's
  downstream consumers have been storing under `~/.config` and `~/.local/share`. (ii) The
  finding names only the two variables, but the port also maps `Desktop`, `MyMusic`,
  `MyPictures`, `MyVideos` and `Templates`, which .NET resolves through
  `~/.config/user-dirs.dirs`. Implementing only the two named variables leaves a half-XDG
  port whose five sibling folders keep ignoring the user's configuration — arguably worse
  than consistent non-support. So the decision is *how far*, not merely *whether*.
* **C3** asks a getter to touch the filesystem: `None` would have to `stat` and return `""`
  for a folder that does not exist (changing the answer for every existing caller whose
  folder is not yet created), and `Create` would have to `mkdir -p` from inside a
  `[[nodiscard]] static std::string` getter. That is a side-effecting accessor, which is a
  design choice, not an oversight.

Both are stated as explicit options in #2320. **Not implemented, not guessed.** In
particular this review does **not** import a remembered desktop-environment convention: the
only XDG semantics treated as known are the ones the frozen finding itself states.

### 4.2 C2 and C4 are evidence-blocked on exact reference text — **#2321, `todo` (deferred)**

Rejecting an undefined `SpecialFolderOption` or an undefined `SpecialFolder` is structurally
decidable from repository-owned data (both enums are declared here). What is *not* decidable
with `/rv` absent is the **exception identity** — type, message and `paramName` — which .NET
raises. That is the same class as **#2252/#2260**: the structural change is known, the exact
text is not, and inventing it would create a divergence no test could later distinguish from
the real thing. Two further facts make deferral the right call rather than a formality:

* C4 needs the definedness table of §3.4, not a guard.
* C2's validation is entangled with C3's semantics — in .NET the option is validated *because*
  it is acted on. Validating an option this port still ignores is coherent input checking but
  is a strictly smaller change than the finding describes, so it should land with C3's
  decision rather than ahead of it.

### 4.3 One thing that is decidable and is deliberately left to the implementer

The header documents the option as "accepted for API compatibility but ignored on POSIX", but
it does **not** state that 32 of the 46 defined folders return `""` on POSIX, nor that XDG is
ignored, nor that `Create` does not create. Recording that reduction is zero-behaviour-change
and needs no approval. It is **not** done here because the frozen finding raises no
documentation clause — inventing one would be scope this review does not have — and because
the statement will have to be rewritten by whichever repair lands. It is noted in #2320 and
#2321 so it lands *with* the repair instead of drifting again.

## 5. Frozen-identifier discipline

No `SR-AUD-*` identifier is created; numbering stays frozen at 364. SR-AUD-105 stays
`confirmed`. SR-AUD-106 is untouched (its state is #2311/#2312/#2313 and is not revisited
here); no inference about the .NET empty-value premise was drawn while reviewing this
finding.
