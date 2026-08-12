<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::IO` (`modules/io`) namespace review — ticket #2097

Owning ticket **#2097**. This document is the durable record; it **remediates nothing by
itself**. Every claim is measured against the tree at `3c28f38`.

`/rv/tmp/runtime/src/libraries/` is **absent** — re-verified 2026-08-04. Every statement about
.NET below comes from repository-contained evidence only: the per-file audit reports, the
doc-comments transcribed from .NET when the module was written, and this module's own tests.
Where a repair would need .NET's exact behaviour and no repository evidence pins it, a
**deferred-verification ticket** is created instead of a guess.

**No `SR-AUD-*` identifier is issued. Audit numbering stays frozen at 364.** Post-audit defects
found by this review carry ordinary ticket numbers only.

CNA and mobile-eggbert were not inspected. Ticket #1773 stays blocked.

Primary evidence: `build-probe/2097_probe1_io_findings.cpp`, log
`build-probe/2097_probe1_before.log`.

---

## 1. Why this unit was selected — re-measured, not inherited

The `System::Net::WebSockets` review §17 recommended `modules/io`. That recommendation was
**re-derived from scratch** against `audit/AUDIT_FINDINGS_INDEX.md` at `3c28f38`, after
`net-websockets` closed, rather than accepted. Every unit with at least six open findings:

| Unit | Open | High | Med | Low | High % | Design-complete | Remediated | Existing review |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| `modules/core` | 72 | 9 | 59 | 4 | 12% | 1 | 47 | family plans only |
| `modules/threading` | 17 | 6 | 11 | 0 | 35% | 0 | 21 | **yes** |
| `modules/runtime` | 14 | 1 | 12 | 1 | 7% | 12 | 8 | **yes** |
| **`modules/io`** | **11** | **0** | **11** | **0** | **0%** | **0** | **2** | **none** |
| `modules/text` | 11 | 1 | 10 | 0 | 9% | 11 | 3 | **yes** |
| `modules/uri` | 10 | 0 | 10 | 0 | 0% | 10 | 4 | **yes** |
| `modules/time-zone` | 7 | 0 | 7 | 0 | 0% | 0 | 0 | none |
| `modules/globalization` | 7 | 1 | 6 | 0 | 14% | 0 | 0 | none |
| `modules/text-json` | 7 | 1 | 6 | 0 | 14% | 1 | 0 | none |
| `modules/net-http` | 6 | 1 | 5 | 0 | 16% | 2 | 3 | **yes** (closed) |

`modules/net-websockets` drops to **4** open with **2** remediated and is closed for compatible
work.

**Unreviewed units with ≥6 open:** `core` (72), `io` (11), `time-zone` (7), `globalization` (7),
`text-json` (7).

### Applying the stated priorities, honestly

1. **High-severity memory or lifetime risk.** `io` has **zero `high` findings**, so on the
   headline metric it *loses* to `text-json` and `globalization` (one each). **This is the
   objection to answer, and the answer is that the severity column understates it**: five of
   `io`'s eleven findings (SR-AUD-337, 342, 343, 344, and 339) are **resource- and
   lifecycle-state** defects — reading and writing through a wrapper that reports itself closed,
   seeking a closed file, mutating the position of a closed stream, and a watch that outlives the
   configuration that created it. Those are the *class* the batch's own priority list ranks first
   (use-after-close, lifetime, resource ownership), recorded at `medium` because each is
   individually contained.
2. **Public-input attackability.** `text-json` parses untrusted documents and is genuinely
   stronger here. `io` handles paths, file contents and descriptors — attackable, but usually by
   a local caller rather than a remote peer.
3. **Decidability without the reference tree.** **This is decisive, and it is where `io` wins
   outright.** `io`'s findings are overwhelmingly self-evident contract violations: a closed
   stream must not keep reading; a negative count must not be accepted; a `std::` exception must
   not escape a `System`-shaped API. `time-zone`'s seven findings and five of
   `globalization`'s seven are *"what exactly does .NET do"* questions — DST rule equality,
   grapheme clusters, culture-aware collation and casing. With `/rv` absent, reviewing those
   units today produces a queue of **deferred-verification tickets**, not compatible work.
4. **Existing family obligation.** `modules/io` holds the **three remaining X-D members**
   (SR-AUD-337/343/344) that both the `System::Xml` review §17 and the `net-websockets` review
   §8.3 name as the **CCF-022 trigger**. No other candidate discharges a recorded obligation.
5. **Coherent module boundary.** One CMake component (`IO`, `TYPE STATIC`,
   `PUBLIC_DEPENDENCIES Core.Base Uri`, `PRIVATE_DEPENDENCIES TimeZone`): 56 public headers
   (3,805 lines), 21 sources (2,950 lines), 5 test files (5,468 lines). Larger than
   `net-websockets` but a single, well-bounded component. `modules/core` at 72 open findings is
   **not** a coherent unit and already has family plans.
6. **No existing complete review.** Correct — none.

**Selected: `modules/io`**, on priorities 3, 4 and 5, with priority 1 explicitly conceded to
`text-json` on the raw severity column and answered in the text above. `modules/text-json` is the
recommended next unit (§17).

---

## 2. Scope and file inventory

| Kind | Files | Lines |
|---|---:|---:|
| public headers | 56 | 3,805 |
| implementation | 21 | 2,950 |
| tests | 5 | 5,468 |

**In scope:** everything under `modules/io/`.

**Out of scope, and why:**

- `IO.Compression`, `IO.Compression.Zip`, `IO.Hashing`, `IO.IsolatedStorage` — separate
  components with their own findings and their own reviews to come.
- `MemoryMappedFiles`, `Pipes` — absent from this port; no finding names them.
- **Asynchronous stream APIs** — `Stream` exposes no `ReadAsync`/`WriteAsync`/`CopyToAsync` in
  this port. The batch's async-lifetime, cancellation-race and callback-after-disposal checklist
  therefore has **no subject here**, and saying so is more useful than inventing one. The only
  callback surface in the module is `FileSystemWatcher`'s event handlers (§7.4).
- `System::IO::Compression`-adjacent exception types re-exported here — declarations only.

---

## 3. Complete public-surface inventory

| Area | Types |
|---|---|
| Stream base | `Stream` (pure-virtual `Read`/`Close`/`getLengthProperty`; `Write`, `Seek`, `SetLength`, `Flush`, `Position`, `CanRead`/`CanWrite`/`CanSeek` virtual with three different defaults — `docs/StreamCapabilityContractDesign.md`) |
| Stream implementations | `FileStream`, `MemoryStream`, `BufferedStream`, `UnmanagedMemoryStream` |
| Binary | `BinaryReader`, `BinaryWriter` |
| Text | `TextReader`, `TextWriter`, `StreamReader`, `StreamWriter`, `StringReader`, `StringWriter` |
| Filesystem statics | `File`, `Directory`, `Path`, `RandomAccess` |
| Filesystem info | `FileSystemInfo`, `FileInfo`, `DirectoryInfo`, `DriveInfo` |
| Watching | `FileSystemWatcher` + 5 event-arg/handler types |
| Unmanaged | `UnmanagedMemoryAccessor`, `UnmanagedMemoryStream` |
| Data | `System::BinaryData` (note: `System/BinaryData.hpp`, **not** under `System/IO/`) |
| Enums / options | `FileMode`, `FileAccess`, `FileShare`, `FileOptions`, `FileAttributes`, `SeekOrigin`, `SearchOption`, `SearchTarget`, `MatchType`, `MatchCasing`, `NotifyFilters`, `WatcherChangeTypes`, `UnixFileMode`, `HandleInheritability`, `FileHandleType`, `EnumerationOptions`, `FileStreamOptions` |
| Exceptions | `EndOfStreamException`, `InvalidDataException`, `FileNotFoundException`, `FileLoadException`, `FileFormatException`, `DriveNotFoundException`, `PathTooLongException`, `InternalBufferOverflowException` |

**Ownership shape worth recording up front:** `StreamReader`/`StreamWriter` take
`Stream*` — a **raw borrowed pointer** — with a `leaveOpen` flag. The wrapper never owns the
stream and nothing keeps it alive. That is a borrowed-pointer edge of the CCF-019 shape; see
§8.1 for why it is recorded rather than ticketed here.

---

## 4. Every open finding, with its measured disposition

All eleven reproduced or refuted against `3c28f38` (`build-probe/2097_probe1_before.log`).

| Finding | Sev | Measured | Disposition | Ticket |
|---|---|---|---|---|
| SR-AUD-185 | med | **confirmed** — `ToString()` of `0xFF` returns `FF`, not `EFBFBD` | deferred verification | **#2106** |
| SR-AUD-186 | med | **premise inverted** — see §6.1 | deferred verification | **#2106** |
| SR-AUD-337 | med | **confirmed** | compatible | **#2098** |
| SR-AUD-339 | med | **confirmed** by reading `setPathProperty` | compatible | **#2102** |
| SR-AUD-340 | med | **confirmed and narrowed** — see §6.3 | compatible | **#2100** |
| SR-AUD-342 | med | **partly refuted, partly confirmed** — see §6.2 | compatible | **#2099** |
| SR-AUD-343 | med | **confirmed** | compatible | **#2098** |
| SR-AUD-344 | med | **confirmed exactly as filed** | compatible | **#2098** |
| SR-AUD-345 | med | **confirmed, and sharper** — see §6.4 | compatible | **#2103** |
| SR-AUD-346 | med | **confirmed** — the inotify mask is a compile-time constant | compatible | **#2102** |
| SR-AUD-347 | med | **confirmed** — raw `std::filesystem_error` escapes | compatible | **#2101** |

### 4.1 SR-AUD-337 / 343 / 344 — a lifecycle state recorded but not enforced

Measured after `Close()`:

- `StreamReader(&ms, /*leaveOpen*/ true)` → `Read()` returns `97` (`'a'`);
- `StreamWriter(&ms, /*leaveOpen*/ true)` → `Write("after close")` succeeds and the underlying
  stream grows to 11 bytes;
- `StringReader("hello")` → `Peek()` = 104, `Read()` = 104, `ReadToEnd()` = `"ello"` — the
  reader is **entirely unaffected**, because `TextReader::Close()` is literally `{}` and
  `StringReader` does not override it;
- `StringWriter` → `Write` after `Close()` succeeds and `ToString()` returns the text;
- `UnmanagedMemoryStream` → `Read()` **does** throw `ObjectDisposedException`, but
  `getLengthProperty()` still returns `4` and `setPositionProperty(2)` still succeeds and reads
  back. **Exactly the split the finding describes.**

### 4.2 SR-AUD-342 — FileStream, see §6.2 for the correction

### 4.3 SR-AUD-339 / 346 — FileSystemWatcher

`setPathProperty` assigns `directory_` and does nothing else — no watch is torn down or
re-established, so while `EnableRaisingEvents` is true the old `inotify` watch stays on the old
directory and its events are reported with a `FullPath` built from the **new** `directory_`.

`setNotifyFilterProperty` validates the mask and stores it; `notifyFilter_` is then **never
read**. The `inotify_add_watch` mask is a `constexpr` constant
(`IN_CREATE|IN_DELETE|IN_MODIFY|IN_ATTRIB|IN_MOVED_FROM|IN_MOVED_TO`).

---

## 5. Structural root-cause families

- **I-A — a public lifecycle state is recorded but not enforced.** SR-AUD-337, 343, 344, and
  SR-AUD-342's `Length`/`Position`/`Seek` half. **This is X-D, the CCF-022 candidate** (§8.2).
- **I-B — a public argument domain is unchecked, or its rejection is untyped.** SR-AUD-340.
- **I-C — a raw `std::` exception escapes a `System`-shaped public API.** SR-AUD-347. Same cause
  as `System::Text`'s T-E/T-M and CCF-012's defect class, in a different component.
- **I-D — a public property is stored, validated, and never consulted.** SR-AUD-346. Same shape
  as `net-websockets`' W-E (SR-AUD-252) and `text-json`'s SR-AUD-326/330.
- **I-E — mutating configuration does not re-establish the resource it configures.**
  SR-AUD-339. No counterpart elsewhere in the audit.
- **I-F — sibling APIs disagree about the same input.** SR-AUD-345 (§6.4). No counterpart.
- **I-G — text decoding and copy semantics diverge from .NET.** SR-AUD-185, 186. Both
  reference-sensitive.

---

## 6. Corrected premises

### 6.1 SR-AUD-186 — the premise is **inverted**, and the port's behaviour is the safer one

The finding says *"C++ preserves byte 01 after its source changes to 02, whereas current .NET
stores/wraps the supplied `ReadOnlyMemory`."* Measured, that is exactly what happens: the port
**copies**, so `BinaryData` still reads `01`.

So the divergence is real, but the finding's implicit direction — that the port is wrong — needs
stating plainly: **.NET's behaviour is the aliasing one, and the port's is the defensive one.**
"Fixing" this means making `BinaryData` **alias caller memory it does not own**, which
introduces a borrowed-pointer lifetime hazard of exactly the CCF-019 shape into a type that today
has none. That is not a repair to make without evidence about which .NET overload is meant
(`BinaryData(byte[])` copies; `BinaryData(ReadOnlyMemory<byte>)` wraps) and without a decision
about whether this port wants the aliasing overload at all. **Deferred, #2106.**

### 6.2 SR-AUD-342 — **half of it is already fixed**, and the surviving half is different

The finding says *"OpenOrCreate+Read can write a new file, and closed members …"*. Measured:

- **`Write()` on a `FileAccess::Read` stream already throws** `"Stream does not support
  writing."` The write half of the finding is **no longer reproducible**. What *is* still true is
  that `OpenOrCreate` + `Read` **creates the file** — but whether that is wrong is a .NET
  behaviour question with no repository evidence, so it is **not** ticketed as a defect.
- **`Read()` after `Close()` already throws** `ObjectDisposedException`.
- **What survives is narrower and was not named:** after `Close()`,
  `getLengthProperty()` returns a **stale `5`**, `getPositionProperty()` returns **`-1`** — a
  sentinel, not an exception — and **`Seek()` succeeds outright**. That is I-A, not a capability
  defect. #2099 is scoped to those three members only.

**Two positives measured and recorded so they are not lost:** over 20 open/close/**close**
cycles the `/proc/self/fd` delta is **0**, and over 20 constructors that throw
(`FileMode::Open` on a missing file) the delta is **0**. `FileStream` **does not leak a
descriptor** on double close or on a throwing constructor. The batch's leading suspicion for this
module was descriptor leaks; measured, there are none at these two sites.

### 6.3 SR-AUD-340 — confirmed, but only one of the two directions is silent

- `RandomAccess::Write(fd, buf, /*count*/ -1, 0)` **succeeds silently**. Confirmed, and it is the
  sharp half.
- `RandomAccess::Read(fd, buf, -1, 0)` and `Read(fd, buf, 2, /*fileOffset*/ -5)` **do throw** —
  but with a bare `"RandomAccess::Read failed"`, i.e. an `errno` failure surfaced as a generic
  message rather than an `ArgumentOutOfRangeException` naming the offending parameter. The
  defect on the read side is the **exception identity**, not the acceptance.
- `RandomAccess::GetLength(-1)` returns **`-1`**. Confirmed.

### 6.4 SR-AUD-345 — confirmed, and the sharper framing is that two siblings disagree

`FileInfo(emptyDirectory).Delete()` **deletes the directory**. But `File::Delete(emptyDirectory)`
**throws** `"Could not delete '…': Is a directory."` Two APIs that .NET documents as equivalent
give opposite answers for the same input in this port, and the finding names only one of them.
The repair is to make `FileInfo::Delete` agree with `File::Delete`, which also means the repair
target already exists in the module.

### 6.5 A probe defect found and fixed rather than reported

The first version of the SR-AUD-343 probe evaluated `Peek()`, `Read()` and `ReadToEnd()` as three
arguments to **one** `printf`. All three **mutate** the reader and their evaluation order is
unspecified, so the output (`Peek()=-1 Read()=-1 ReadToEnd()="hello"`) was meaningless — it
recorded `ReadToEnd()` having run first. Sequenced into three statements, the real answer is
`Peek()=104 Read()=104 ReadToEnd()="ello"`, which is a **stronger** confirmation than the
garbled version. Recorded because a measurement taken through UB is not a measurement.

---

## 7. Post-audit observations (no `SR-AUD-*` identifier)

1. **`TextReader::Close()` is `{}` and `TextWriter` has no `Close()` at all** — the base classes
   have no disposal contract, which is *why* SR-AUD-343 exists. Any I-A repair has to decide
   whether the state lives in the base or in each leaf. §9 records this as the first design
   question #2098 must answer.
2. **`StreamReader`/`StreamWriter` hold a raw borrowed `Stream*`.** A wrapper outliving its
   stream is a use-after-free with no diagnostic. **CCF-019 shape**; recorded in §8.1, not
   ticketed here.
3. **`Stream`'s three-different-defaults virtual surface** (`CanRead` defaults **true**,
   `CanWrite` defaults **false**, `Read`/`Close`/`getLengthProperty` pure) is already documented
   in `docs/StreamCapabilityContractDesign.md`. Not re-opened.
4. **`FileSystemWatcher` is the module's only callback surface**, and its handlers are invoked
   from the watcher thread. Whether a handler can run *after* `EnableRaisingEvents = false`
   returns is a concurrency question this review did **not** measure, because doing it properly
   needs TSan plus a deterministic harness. Recorded as **#2105, deferred**, not asserted.
5. **`RandomAccess` takes a raw `int fd`** with no ownership or validity concept; `GetLength(-1)`
   returning `-1` is the visible symptom (§6.3).

---

## 8. CCF mapping

### 8.1 CCF-019 — `modules/io` adds a candidate edge, and it is NOT ticketed here

`StreamReader`/`StreamWriter`'s raw borrowed `Stream*` is the family's shape. It is **recorded,
not ticketed**, for the same reason `net-websockets` gave: #2066 is the family's open design
question with **two competing options and no selection**, and adding a seventh local answer would
defeat the purpose of the family. **CCF-019 remains open**; nothing here closes it.

### 8.2 CCF-022 — the trigger is met, the evidence is complete, and it is **still not minted**

X-D — *a public lifecycle state recorded but not enforced*:

| Module | Finding / site | State |
|---|---|---|
| `xml` | SR-AUD-349 (`XmlWriter` after `Close`) | **remediated**, #2076 |
| `xml` | SR-AUD-348 (`XmlReader::Close` unobservable) | **remediated**, #2078 |
| `io` | SR-AUD-337 (`leaveOpen` text wrappers) | confirmed, #2098 |
| `io` | SR-AUD-343 (in-memory text wrappers) | confirmed, #2098 |
| `io` | SR-AUD-344 (`UnmanagedMemoryStream`) | confirmed, #2098 |
| `io` | SR-AUD-342 `Length`/`Position`/`Seek` half | confirmed, #2099 |

Six sites, two modules, one structural cause, and one governing policy: *a type that records
being closed must enforce it at every public member that depends on the closed resource, not
only at the data-transfer ones.* The `System::Xml` review §17 and the `net-websockets` review
§8.3 both name **"mint CCF-022 when `modules/io` is reviewed"** as the trigger. **That trigger is
now met.**

**It is nonetheless NOT minted by this review, and the reason is a contradiction in the durable
record that belongs to the maintainer, not to me.** `audit/AUDIT_CROSS_CUTTING_FINDINGS.md`
states in **five** separate namespace appendices that *"the cross-cutting numbering is closed"*.
Every one of those statements is made while declining to promote a **namespace-local** cause, so
they do not obviously govern a genuinely cross-module family — but they are unqualified, and
`AUDIT_CROSS_CUTTING_FINDINGS.md` is the authoritative document for CCF minting. Minting
CCF-022 would require reading past an unqualified "closed" on the strength of an inference.

**What this review does instead:** records the membership above as complete, so that minting is a
one-paragraph act whenever the maintainer resolves the contradiction. The same is true of
CCF-021 (`net-websockets` review §8.2, and the cross-cutting appendix added by that batch).
**Neither is minted. No finding is marked remediated on account of either.**

Note also that **the number `CCF-021` has been proposed for two different candidate families** —
the `SearchValues.hpp` per-file report proposes it for a public-generic-surface shape that
`AUDIT_PROGRESS.md` later declines. Whoever mints must say which family the number names.

### 8.2.1 CORRECTION, added by the #2100/#2107/#2098-design batch — the policy contradiction is resolved

§8.2's stated reason for not minting — an unqualified "the cross-cutting numbering is closed" in
`audit/AUDIT_CROSS_CUTTING_FINDINGS.md` — **no longer stands**, and §8.2's text above is left
unedited so the reasoning at the time stays visible.

Measured against that document's own text, **"the numbering is closed" is scoped to
namespace-local causes**, not a global freeze: six of its seven occurrences say so in the same
sentence, and the same document instructs *"Mint CCF-021 when `net-http-headers` is reviewed"*
and names this review as CCF-022's trigger. A global freeze and a written instruction to mint
later cannot both be literal. The full argument, its honest counter-example, and CCF-022's
re-enumerated membership are in that file's **"Numbering-policy reconciliation"** appendix.

**CCF-022 is still not minted**, but for a different and narrower reason: every promotion
sentence in the repository is passive and names no agent, so *who* may execute a mint is genuinely
unsettled, and two of the family's six sites are now blocked behind Approval IO-1 (§21.8), which
would make the family unclosable on the day it was minted. That is a real decision with more than
one defensible answer, so it is **decision ticket #2109** with three bounded options, a
recommendation and an exact approval sentence — not something this batch resolves by inference.

**One member has moved since §8.2 was written:** SR-AUD-344 is **remediated** by #2108, so the
family now stands at **three remediated of six**.

### 8.3 CCF-012

Not a member. SR-AUD-347 shares CCF-012's *defect class* (a `std::` primitive's exception
escaping a `System`-shaped API) but CCF-012 is about **composite-format brace grammars**.
Recorded so a future reader does not merge them. **CCF-012 is not marked closed.**

---

## 9. Dependency graph of the tickets

```
#2098  enforce the closed state in the text wrappers and UnmanagedMemoryStream  (P2, M)
         └─ answers the base-vs-leaf design question §7.1 FIRST; #2099 follows its answer
#2099  FileStream Length/Position/Seek after Close                              (P2, S) ── AFTER #2098
         └─ CORRECTED 2026-08-12 by #2099: the "AFTER #2098" edge was a STORAGE-decision
            dependency, and FileStream has no storage decision to inherit -- file_.is_open()
            already IS the closed flag. Measured layout-neutral (sizeof 576 before and after),
            so #2099 landed WITHOUT Approval IO-1, exactly as #2108 did. #2098 is unaffected
            and stays blocked on that approval for its four text wrappers.
#2100  RandomAccess argument domain and GetLength                               (P2, S) ── independent
#2101  empty-path FileSystemInfo leaks std::filesystem_error                    (P2, S) ── independent
#2102  FileSystemWatcher: NotifyFilter ignored, Path does not re-arm            (P2, M) ── independent
#2103  FileInfo::Delete disagrees with File::Delete over a directory            (P3, S) ── independent
#2104  documentation and gated-behaviour pins for the deferred/blocked items    (P3, S) ── LAST
#2105  DEFERRED: can a watcher handler run after EnableRaisingEvents = false?   (P3)
#2106  DEFERRED: BinaryData decoding (185) and copy-vs-wrap semantics (186)     (P3)
```

---

## 10. Compatible versus blocked or deferred

| Ticket | Compatible? | Why |
|---|---|---|
| #2098 | **yes, with a documented narrowing** — but see §11: the *storage* question decides it |
| #2099 | **yes, with a documented narrowing** — three members start throwing |
| #2100 | **yes, with a documented narrowing** — a negative count stops being accepted |
| #2101 | **yes** — an exception type changes from `std::filesystem_error` to a `System` type |
| #2102 | **yes, with a documented narrowing** — filtered events stop arriving |
| #2103 | **yes, with a documented narrowing** — `FileInfo::Delete` stops deleting directories |
| #2104 | **yes** — documentation and pins only |
| #2105, #2106 | **no** — deferred verification |

**Nothing here is blocked on a user approval.** That is the practical payoff of §1's priority-3
argument, and it is the strongest contrast with `time-zone` and `globalization`.

---

## 11. Source / ABI / layout / vtable / `noexcept` consequences

**This is the section that decides #2098's shape, and it must be settled before implementation.**

| Ticket | Source | ABI / layout | vtable | `noexcept` |
|---|---|---|---|---|
| #2098 | narrows | **SEE BELOW — a `closed_` flag is a LAYOUT CHANGE** | see below | none |
| #2099 | narrows | **`FileStream` already has a closed flag** — verify before implementing | none | none |
| #2100 | narrows | none | none | none |
| #2101 | changes an escaping exception's type | none | none | none |
| #2102 | narrows | none (both fields already exist) | none | none |
| #2103 | narrows | none | none | none |

**The layout question.** Enforcing a closed state needs somewhere to record it. Three options,
and #2098 must pick one **and say so**:

- **(a) a `bool` in each leaf** (`StringReader`, `StringWriter`, `StreamReader`, `StreamWriter`,
  `UnmanagedMemoryStream`) — an object-layout change in five public types;
- **(b) a `bool` in `TextReader`/`TextWriter`** — an object-layout change in two **base** classes,
  which changes the layout of every derived type at once and is the more invasive option;
- **(c) reuse state each type already has** — e.g. `StringReader` could set its position past the
  end, `StreamReader`/`StreamWriter` could null their `Stream*`. **No new member, no layout
  change**, but it conflates "closed" with "at end", which is exactly the kind of aliasing that
  produces the next finding.

Every option is layout-affecting except (c), and (c) is semantically wrong. **#2098 is therefore
recorded as compatible *in behaviour* but as requiring an object-layout change**, and it must be
implemented with a layout pin updated in the same change, following the probe-struct technique
(`docs/SystemNetWebSocketsNamespaceReviewPlan.md` §11) rather than literal byte counts.

**This is a real gate and it is stated rather than discovered during implementation.**

---

## 12. Observable semantic consequences

- **#2098/#2099** — code that reads or writes through a wrapper after `Close()` starts throwing
  `ObjectDisposedException`. Code relying on `leaveOpen` to keep *the wrapper* usable breaks;
  that reliance was on a bug.
- **#2100** — `RandomAccess::Write` with a negative count throws instead of silently doing
  nothing.
- **#2101** — an empty-path `FileInfo`/`DirectoryInfo` throws a `System` exception instead of a
  `std::filesystem_error`. A caller catching `System::Exception` starts catching what it always
  should have.
- **#2102** — a watcher configured with `NotifyFilters::Size` stops raising `Created`. Callers
  that set a filter and relied on getting everything anyway will see fewer events.
- **#2103** — `FileInfo::Delete` over a directory throws instead of deleting it. **This one
  removes a data-loss path**, and is the only finding in the module that destroys user data.

A migration note (`docs/Migration-IOLifecycleAndArgumentStrictness.md`) covers #2098–#2103
together when the first of them lands.

---

## 13. Test matrix

| Ticket | Required cases |
|---|---|
| **#2098** | every public member of `StringReader`, `StringWriter`, `StreamReader`, `StreamWriter`, `UnmanagedMemoryStream` after `Close()`; `leaveOpen` **true and false** for both stream wrappers; the underlying stream still usable when `leaveOpen` is true; double `Close()`; `Close()` then destructor; zero-length reads/writes before and after; the layout pin |
| **#2099** | `Length`, `Position` (get **and** set), `Seek` (all three `SeekOrigin`s), `SetLength`, `Flush` after `Close()`; double `Close()`; **descriptor count unchanged** across all of them |
| **#2100** | `count` = −1, 0, 1; `fileOffset` = −1, 0, past EOF; `buffer` null with count 0 and with count > 0; exact exception type **and** `paramName`; `GetLength` on −1 and on a valid fd; a valid read/write still works |
| **#2101** | `FileInfo("")`, `DirectoryInfo("")`, `FileSystemInfo` subclasses: `Exists`, `Length`, `Create`, `Delete`, `Refresh`, `FullName`; **no `std::` exception escapes any of them**; a whitespace-only path; a valid path unaffected |
| **#2102** | each `NotifyFilters` value alone: an event that matches arrives, one that does not is absent; `Path` changed while enabled re-arms on the new directory and stops reporting the old; `Path` changed while disabled; deterministic synchronisation only — **no sleeps** |
| **#2103** | `FileInfo(dir).Delete()` over an empty **and** a non-empty directory; `FileInfo(file).Delete()` still works; a missing path is still a no-op if that is the current contract; `File::Delete` unchanged; **the directory still exists afterwards** |
| **pins** | `TextReader`/`TextWriter`/`StringReader`/`StringWriter`/`StreamReader`/`StreamWriter`/`UnmanagedMemoryStream`/`FileStream` layout probes; the measured no-leak result of §6.2; #2105's and #2106's current behaviour |

---

## 14. Sanitizer and direct-resource matrix

| Tool | Applicable here? |
|---|---|
| **ASan** | yes — `UnmanagedMemoryStream` and `UnmanagedMemoryAccessor` index caller memory; `BinaryReader`/`BinaryWriter` and `MemoryStream` do buffer arithmetic |
| **UBSan** | yes — `#2100`'s negative counts and offsets, and every `intcs`/`longcs`/`size_t` conversion in the seek and length arithmetic |
| **LSan** | **weakly** — the module's characteristic resource is a **descriptor**, not heap |
| **TSan** | only for `FileSystemWatcher` (#2102, #2105). Nothing else in the module is concurrent |
| **`/proc/self/fd`** | **the primary instrument.** LSan must never be substituted for it: it tracks memory, not descriptors, and a clean LSan run says nothing about a leaked `fd` |

Measured already (§6.2): **zero** descriptor delta over 20 `FileStream` double-close cycles and
20 throwing constructors. Every ticket that touches `FileStream` or `RandomAccess` must re-measure
this and report the number, not assert it.

---

## 15. Recommended implementation order

1. **#2101** — smallest, fully independent, and removes a raw `std::` exception from a public API.
2. **#2100** — small, independent, closes a silently-accepted negative count.
3. **#2103** — small, independent, and the only finding that **destroys user data**.
4. **#2098** — the CCF-022 core. Needs §11's storage decision and a layout pin.
5. **#2099** — after #2098, following whatever storage answer it chose.
6. **#2102** — largest; needs a deterministic watcher harness.
7. **#2104** — documentation and pins, last.

**Items 1–3 are the recommended subset for a batch that has already spent most of its context**,
because each is bounded, independent, and needs no layout decision.

---

## 16. Deferred evidence

`/rv/tmp/runtime/src/libraries/` is absent. These are **not** decided by this review:

- `BinaryData`'s decoding of invalid UTF-8, and whether the copying or the wrapping constructor
  is the one meant (#2106);
- whether `FileMode::OpenOrCreate` with `FileAccess::Read` should create the file (§6.2) — the
  port creates it, and no repository evidence says otherwise, so **nothing is changed**;
- whether a `FileSystemWatcher` handler can be invoked after `EnableRaisingEvents = false`
  returns (#2105);
- the exact exception type and `paramName` .NET raises for a negative `RandomAccess` count —
  #2100 chooses `ArgumentOutOfRangeException` with the parameter named and **records it as this
  port's choice**.

---

## 17. Recommended next unit

**`modules/text-json`** — 7 open, 1 high, a parser over untrusted input, and one coherent
component. Its high finding (SR-AUD-327) is CCF-019 and blocked, but the `net-websockets` review
demonstrated that a blocked top finding does not make a unit's compatible queue worthless.
`modules/net-http-headers` remains the **CCF-021 trigger** at 5 open findings (two `high`),
below the ≥6 threshold but carrying a recorded family obligation — reviewing it would let
CCF-021 be minted with all five members present.

---

## 18. Exclusions

- The four sibling `IO.*` components — separate reviews.
- Asynchronous stream APIs — **absent from this port** (§2), so the async-lifetime and
  cancellation-race checklist has no subject.
- `docs/StreamCapabilityContractDesign.md`'s virtual-default decisions — settled, not reopened.
- CNA and mobile-eggbert — not inspected; #1773 stays blocked.

---

## 19. Completion criteria

This review (#2097) is complete when this document exists, each of the eleven open findings has
exactly one disposition in §4, each post-audit observation carries a ticket or an explicit
"recorded, not ticketed", and §9's tickets are in `plan.sqlite3`. **It is complete on those terms
and remediates nothing by itself.**

`modules/io` is closed for *compatible* work when #2098–#2104 are `done`, SR-AUD-337, 339, 340,
342, 343, 344, 345, 346 and 347 are `remediated`, and SR-AUD-185/186 carry a deferred ticket and
a pin.

---

## 20. Implementation record

Appended as tickets land, so the difference between what this review predicted and what
implementation measured stays visible.

### 20.1 #2101 landed, and its real scope was ONE door, not seven

§13's test matrix and the review's own grep expected several escape routes. Measured
(`build-probe/2101_probe2_before.log`), **all but one were already guarded**:
`DirectoryInfo::GetFiles`/`GetDirectories`/`GetFileNames` over a missing directory already threw
a `System` exception, and `FileInfo::CopyTo("")`/`DirectoryInfo::MoveTo("")` already threw. The
**only** real leak was the `FileSystemInfo` **constructor**, where
`std::filesystem::absolute(path)` on an empty path threw
`std::filesystem::filesystem_error("cannot make absolute path: Invalid argument")` straight out
of `FileInfo("")` and `DirectoryInfo("")`.

Narrowing the ticket to what was measured — rather than "fixing" six sites that were not broken —
is the result, and the already-guarded neighbours are now **pinned by a test** so they stay that
way.

The repair rejects the empty case explicitly and routes **every other** failure through the
`std::error_code` overload, so `filesystem_error` can no longer escape this door **by
construction** rather than by being caught.

**A whitespace-only path is deliberately still accepted.** It resolves cleanly on POSIX and no
repository evidence says .NET rejects it here; narrowing on a guess is exactly what the
deferred-verification rule exists to prevent. **Pinned by a test** so that changing it is a
decision rather than a drift.

Exception identity — `System::ArgumentException` with `paramName` `"path"` and the wording
`FileInfo::CopyTo` already uses for the same defect in a different parameter — is **this port's
choice**, recorded as such.

### 20.2 #2103 landed by routing the broken door through the sibling that was already right

§6.4's correction was the whole repair. `File::Delete` **already** had the guard, complete with a
comment recording that real .NET calls `unlink()`, which can never remove a directory.
`FileInfo::Delete` called `std::filesystem::remove` directly, and `std::filesystem::remove` is
documented to call the equivalent of `rmdir` on a directory — so it **silently deleted the
directory**. The repair delegates to `File::Delete` rather than writing a second copy of the
rule: one guard, one message, and the two siblings cannot drift apart again.

This required adding `#include "System/IO/File.hpp"` to `FileInfo.hpp`. No new component edge:
both are in `IO`. The module graph is unchanged at **41 modules and 91 edges**.

### 20.3 #2100 landed, and the ORIGINAL REPORT was broader than both the ticket and §6.3

**The most important correction this ticket produced is that §6.3 under-described SR-AUD-340.**
§6.3 was written from the finding's index summary — *"negative `Write` counts silently succeed
and POSIX `GetLength` returns −1 for invalid descriptors"* — and the ticket inherited that
scope. Reading the **owning per-file report**
(`audit/modules/io/src/System/IO/RandomAccess.cpp.audit.md`) before editing production code shows
it already names four more things:

> *"The raw pointer/count overloads do not validate **pointer, count, or offset**"* … *"they omit
> invalid descriptors, **null buffers**, negative counts/offsets/**lengths**, **read-only
> descriptors**, **non-seekable handles**, and **write-zero-progress** behavior."*

So the repair surface is not two members but **the whole class**, and the review's own §6.3
narrowing would have delivered a partial fix. Measured before any edit
(`build-probe/2100_probe1_before.log`):

| Door | Before | After |
|---|---|---|
| `Write(count = -1)` | **returned normally** — the sharp half | `ArgumentOutOfRangeException("count")` |
| `Read(count = -1)` | `IOException("RandomAccess::Read failed")` | `ArgumentOutOfRangeException("count")` |
| `Read(fileOffset = -5)` | same bare `IOException` | `ArgumentOutOfRangeException("fileOffset")` |
| **`Write(fileOffset = -5)`** | same bare `IOException` — **never named by the finding's summary** | `ArgumentOutOfRangeException("fileOffset")` |
| **`SetLength(length = -1)`** | same bare `IOException` — **never named by the summary** | `ArgumentOutOfRangeException("length")` |
| **`Read`/`Write(buffer = nullptr, count > 0)`** | same bare `IOException` (an `EFAULT`) | `ArgumentNullException("buffer")` |
| `Read`/`Write(buffer = nullptr, count == 0)` | returned normally | **unchanged** — nothing is transferred |
| `GetLength(-1)` | **returned the sentinel `-1`** | `IOException` naming `EBADF` |
| **`GetLength(pipe)`** | **returned `-1` for a perfectly VALID descriptor** | `IOException` naming `ESPIPE` |
| every native failure | reason discarded — `EBADF`, `EINVAL` and `ENOSPC` were indistinguishable | the operating system's own text is appended |
| the `std::vector` overloads | `size()` narrowed to `intcs` unchecked | oversized buffers rejected; the rest inherited |

**`GetLength` is wider than "an invalid descriptor".** The finding says *invalid*; measured, a
valid pipe returns `-1` too, because `lseek` fails with `ESPIPE` and all three results were
discarded. The repair covers both, and **the mechanism is deliberately unchanged** — `lseek`
still supplies the answer and the original position is still restored, so every descriptor that
worked before returns exactly the same number. Switching to `fstat()` would also answer for
pipes, but that is a behaviour change on a guess with the reference tree absent, so it is not
made. Pinned by a test.

**The Windows branch of `GetLength` already threw.** This is a POSIX-only defect, the same
"the sibling was already right" shape as #2103.

**Two robustness defects the report names and the summary does not, both repaired:**
`pwrite`/`pread` interrupted by a signal threw — for `Write`, from the middle of the loop, leaving
the bytes already written in place with no way for a `void` return to report them — and a
zero-progress write would have **spun the loop forever** because `count` never decreases. Both
now behave: `EINTR` retries (the idiom `FileSystemWatcher.cpp` already uses), zero progress
throws. **Neither is covered by a deterministic test** — signal delivery mid-`pwrite` and a
zero-byte `pwrite` on a regular file cannot be forced in this environment — and they are
recorded as inspection-verified rather than claimed as tested.

**Exception identity is this port's choice**, per §16, and is now also stated in the header's
own doc-comment so a caller reads the contract where they use the API.

### 20.4 Evidence

**+12 permanent regressions, add-only** — `SharpRuntimeTests_IO` 599 → **611**, in a new
`IONamespaceReviewTests` suite. Fixtures live under the repository-local `build-tmp/`, never
`/tmp`, per the build-resource policy, and each is uniquely named so two runs cannot collide.

**Two mutations**, each reverted from an exact backup with `git diff --stat` identical on both
sides and no marker surviving: O1 (the original throwing `absolute()`, no guard) → exactly **4**
tests; O2 (the original `std::filesystem::remove`) → exactly **2**.

**A first mutation attempt was DISCARDED rather than reported.** Removing the empty-path guard by
commenting out its `if` left unbalanced braces, so the build failed — and the test run that
followed used the **stale binary** and reported all 611 passing. A stale-binary run is not
discrimination evidence. The mutation harness was changed to **check the build succeeded before
trusting the result**, and O1 was re-done as a faithful restoration of the original expression.

**One test is honestly recorded as non-discriminating.**
`FileInfoDeleteOverANonEmptyDirectoryThrowsAndItsContentsSurvive` passes with O2 applied as well,
because `std::filesystem::remove` fails with `ENOTEMPTY` on a non-empty directory anyway. It
asserts a true and worthwhile invariant, but it is **not** evidence for this guard — the
empty-directory case is.

**Sanitizers**: ASan/UBSan/LSan clean over **600 rejections and 400 acceptances**, with a control
heap-buffer-overflow proving instrumentation was live (`build-probe/2101_probe3_asan.log`). Both
repaired bodies are **header-only `inline`**, so they are compiled into the probe TU and
instrumented by construction — there is no STATIC-archive blind spot here.

**Descriptor accounting** is not applicable to these two tickets (neither opens a descriptor);
§6.2's measured zero-leak result for `FileStream` is unaffected and untouched.

No signature, member, base-class, virtual, vtable, object-layout or exception-specification
change.

### 20.5 #2100's evidence

**+15 permanent regressions, add-only** — `SharpRuntimeTests_IO` 611 → **626**, in a new
`RandomAccessFixture` inside the existing `IONamespaceReviewTests` suite. Fixtures live under the
repository-local `build-tmp/`, never `/tmp`.

**Five mutations**, each restoring an exact piece of the original defect, all applied to the
**final shipped source** and all verified to have built, rebuilt the binary and run
(`build-probe/2100_mutations.py`, log `build-probe/2100_mutations.log`):

| Mutation | Restores | Distinct tests failed |
|---|---|---:|
| M1 | `Write` validates nothing | **7** |
| M2 | `Read`'s rejection is an untyped `IOException` again | **5** |
| M3 | `GetLength` returns the `-1` sentinel again | **4** |
| M4 | `SetLength` stops validating `length` | **2** |
| M5 | the native reason is discarded again | **2** |

The unmutated control failed **0**, and the source was restored **byte-identical**
(md5 `941ccba9…` on both sides). M2 is the load-bearing one: `Read` *already threw* before
#2100, so a bare `EXPECT_THROW` would have passed against the old code. What discriminates is
`ExpectThrowsNaming`, which asserts the exception's `getParamNameProperty()`.

**A stale-binary false pass fired and was caught.** The first mutation harness restored its
backup with `cp -p`, which preserves the backup's mtime; the restored source then looked *older*
than the object file built from the mutant, the build skipped it, and the run reported the
**mutant's** result as the repaired tree's. The harness now never preserves mtime on restore and
verifies the binary was actually rebuilt. This is the second batch running in which this exact
trap has fired — it is a property of the harness, not of a particular ticket.

**Sanitizers: ASan + UBSan + LSan clean over 3,900 rejections and 1,300 acceptances**
(`build-probe/2100_probe2_san.log`). `RandomAccess.cpp` is a real member of
`libsharp_runtime_io.a`, **not** a header-only inline body, so it is compiled *into* the
instrumented translation unit (`build-probe/2100_san_compile.sh`); linking the prebuilt archive
would have left the repaired code uninstrumented and the clean run worthless. **Both controls
fire**: the ASan control's stack trace names `RandomAccess.cpp:172`, proving the repaired body is
the instrumented one, and the UBSan control reports the deliberate signed overflow. The ASan
control's *first* version did **not** fire — it read into a heap buffer from a freshly truncated
file, so `pread` transferred nothing and there was no overflow. That result was discarded and the
control fixed rather than reported.

**Direct descriptor accounting**, per §14 and never LSan: `/proc/self/fd` delta is **0** across
200 rejected calls in the probe and across 200 more in a permanent test, and the descriptor stays
fully usable after repeated rejections.

**No public signature, member, base-class, virtual, vtable, object-layout or exception-specification
change.** The only header change is doc-comments. No component edge changes; the graph stays at
**41 modules / 91 edges**.

---

## 21. #2098 — the measured layout decision and its approval package

**Nothing in this section is implemented.** It exists because §11 declared a layout gate without
measuring it, and the measurement changes the answer. Evidence:
`build-probe/2098_probe1_layout.cpp` (log `2098_probe1_layout.log`) and
`build-probe/2098_probe2_traits.cpp` (log `2098_probe2_traits.log`), both measured at `88224c0`.

### 21.1 Three premises in §7.1, §11 and the ticket are WRONG, and each narrows the decision

1. **"`TextWriter` has no `Close()` at all."** It has one:
   `TextWriter.hpp:107`, `virtual void Close() {}` — exactly like `TextReader.hpp:37`. **Both**
   base classes already expose a virtual, non-pure `Close()`. So no base needs a new virtual, and
   **overriding `Close()` in a leaf adds no vtable slot**. What the bases lack is *state*, not the
   *hook*.
2. **"Every option is layout-affecting except (c)."** Measured, option (a) is layout-**neutral**
   for **four of the five** types, because the bool lands in existing tail padding.
3. **"a `bool` in each of five leaves."** Only **four** leaves would need one.
   **`UnmanagedMemoryStream` already has `isOpen_`**, `Close()` already clears it, and
   `getCanRead`/`getCanWrite`/`getCanSeek` already consult it. Its repair is **pure logic with
   zero layout impact** — which is why §21.6 splits it out.

### 21.2 Current layout, measured

| Type | `sizeof` | `alignof` | Fields, in declaration order |
|---|---:|---:|---|
| `TextReader` | **8** | 8 | *none* — vtable pointer only |
| `TextWriter` | **8** | 8 | *none* — vtable pointer only |
| `Stream` | **8** | 8 | *none* — vtable pointer only |
| `StringReader` | **48** | 8 | `std::string s_` (32); `intcs pos_` (4) |
| `StringWriter` | **384** | 8 | `std::ostringstream buf_` (376) |
| `StreamReader` | **24** | 8 | `Stream* stream_`; `bool leaveOpen_`; `bool ownsStream_`; `bool hasPeeked_`; `bytecs peeked_` |
| `StreamWriter` | **24** | 8 | `Stream* stream_`; `bool leaveOpen_`; `bool ownsStream_` |
| `UnmanagedMemoryStream` | **40** | 8 | `bytecs* buffer_`; `intcs length_`; `intcs capacity_`; `intcs position_`; `FileAccess access_`; **`bool isOpen_`** |

Every field above is **private**. `TextReader`/`TextWriter`/`Stream` declare no data at all;
`UnmanagedMemoryStream` has a `protected` default constructor and `Initialize`.

**Spare tail padding today:** `StreamReader` uses 12 of 24 bytes (**12 spare**); `StreamWriter`
uses 10 of 24 (**14 spare**); `StringReader` uses 44 of 48 (**4 spare**); `StringWriter` uses all
384 (**0 spare**).

### 21.3 The three options, measured

| Type | Now | **(a)** bool per leaf | **(b)** bool per base | **(c)** reuse existing state |
|---|---:|---:|---:|---|
| `TextReader` (base) | 8 | **8** | **16** | 8 |
| `TextWriter` (base) | 8 | **8** | **16** | 8 |
| `StringReader` | 48 | **48** | **56** | 48 |
| `StringWriter` | 384 | **392** | **392** | 384 |
| `StreamReader` | 24 | **24** | **32** | 24 |
| `StreamWriter` | 24 | **24** | **32** | 24 |
| `UnmanagedMemoryStream` | 40 | **40** (no new field) | 40 | 40 |
| **Types whose layout changes** | — | **1** | **6** | **0** |

**Option (a) costs exactly one type eight bytes: `StringWriter`, 384 → 392 (+2.1%).** Option (b)
changes six, including both base classes — which changes the layout of *every* type derived from
them, in this module and in any consumer. Option (c) remains semantically wrong for the reason
§11 gives: it conflates "closed" with "at end", and for `StreamReader`/`StreamWriter` it would
mean nulling a `Stream*` that `leaveOpen` semantics still need.

### 21.4 Consequences, by category

| Category | Option (a), the recommendation |
|---|---|
| **Source** | Narrows: six members of `StringReader`, one of `StringWriter`, the read/write surface of both stream wrappers, and six of `UnmanagedMemoryStream` begin throwing `ObjectDisposedException` after `Close()`. No signature, name or overload set changes. |
| **Object layout** | **One type only**: `StringWriter` 384 → 392. The other four absorb the flag into existing padding, and `UnmanagedMemoryStream` needs no new field. |
| **ABI / mangled symbols** | **No mangled name changes.** Adding a private data member never alters a mangled symbol. But a `sizeof` change is still a **recompilation requirement**: a translation unit compiled against the old header and linked against a new one is an ODR violation with no diagnostic. sharp-runtime ships as a **static library built from source**, so this forces a full consumer rebuild rather than breaking a distributed binary — a real cost, and not the same cost as a shared-library ABI break. **A private field is not automatically ABI-neutral, and this document does not claim it is.** |
| **vtable** | **Unchanged.** `TextReader::Close()` and `TextWriter::Close()` both already exist and are virtual and non-pure, so overriding them adds no slot. Adding any *new* virtual (a `getIsClosedProperty`, say) **would** add one, and is therefore not proposed. |
| **`noexcept`** | Unchanged. No member involved is declared `noexcept`; members that begin throwing were already permitted to throw. |
| **Copy / move** | Unchanged by the flag itself — see §21.5 for a pre-existing hazard it does not create. |
| **Thread safety** | Unchanged. None of these types is thread-safe today and none becomes so. |
| **Performance / allocation** | One predictable branch per call; no allocation. `StringWriter` grows one cache-line-irrelevant 8 bytes on a 384-byte object. |
| **Migration** | `docs/Migration-IOLifecycleAndArgumentStrictness.md` (§12), plus a full consumer rebuild. |

### 21.5 A pre-existing hazard found while measuring, recorded and NOT ticketed here

`StreamReader` and `StreamWriter` are **implicitly copy-constructible and copy-assignable**
(measured, `2098_probe2_traits.log`) while holding a raw `Stream* stream_` **and** an
`ownsStream_` flag. Copying one produces two objects that both believe they own the same stream.
This is a **CCF-019-shaped** defect, it is **not created by #2098**, and it is **not in #2098's
scope**. It is recorded here so the next reader does not have to rediscover it. `TextWriter` and
`StringWriter` are already non-copy-constructible, so the family is not even self-consistent.

### 21.6 The compatible split — real, not invented

**`UnmanagedMemoryStream` (SR-AUD-344) is independently separable and needs no layout decision at
all.** It is a distinct finding over a distinct type; the bundling of SR-AUD-337, 343 and 344
into one ticket was this review's choice, not a property of the defects. Measured, its flag,
its `Close()` and three of its capability properties **already exist and already work**; six
members simply never consult them:

| Member | Today, after `Close()` | Should |
|---|---|---|
| `getLengthProperty()` | returns `4` | throw `ObjectDisposedException` |
| `getCapacityProperty()` | returns the capacity | throw |
| `getPositionProperty()` | returns the position | throw |
| `setPositionProperty()` | **succeeds** | throw |
| `getPositionPointerProperty()` | **hands out a raw pointer into the buffer** | throw |
| `Flush()` | silently succeeds | throw |
| `SetLength()` | throws, but checks `value < 0` **before** `isOpen_` | check `isOpen_` first |

`Read`, `Write` and `WriteByte` are **already correct**. And this is decidable **without the
reference tree**: `UnmanagedMemoryStream.cpp`'s own transcribed comment already records that
.NET's `EnsureNotClosed()` is checked **first** and throws
`ObjectDisposedException("Cannot access a closed Stream.")` — repository-contained evidence, not
a guess.

**Split as ticket #2108**, compatible, zero layout. **#2098 keeps SR-AUD-337 and SR-AUD-343** —
the four text wrappers — and stays gated on the approval below.

### 21.7 Rollback

Option (a) is one private field per leaf plus guard clauses. Reverting is a clean revert of one
commit; no data format, no persisted state and no serialized layout is involved.

### 21.8 The exact approval sentence

> **Approval IO-1.** Approve enforcing the closed state in `System::IO::StringReader`,
> `StringWriter`, `StreamReader` and `StreamWriter` by adding one **private `bool`** to each of
> those four leaf classes — **not** to `TextReader` or `TextWriter` — so that every public member
> depending on the closed resource throws `ObjectDisposedException` after `Close()`, accepting
> that **`sizeof(StringWriter)` grows from 384 to 392 bytes while the other three are unchanged**,
> that every consumer must be **fully recompiled** because a `sizeof` change across a
> stale-header boundary is an undiagnosed ODR violation, and that code relying on `leaveOpen` to
> keep the *wrapper* usable after `Close()` stops working. **No vtable, mangled-symbol,
> signature or `noexcept` change is involved.** Ticket **#2098**.
>
> *(`UnmanagedMemoryStream`/SR-AUD-344 is **excluded** and needs no approval — it already has the
> state and is split out as **#2108**. Answer separately if you prefer option (b), a `bool` in
> `TextReader`/`TextWriter`, which changes **six** types' layout including both bases and every
> type derived from them, in exchange for one flag definition instead of four.)*

**Recommendation: option (a).** It is the measured minimum — one type's layout instead of six —
and the "one flag in one place" appeal of option (b) is worth less than the layout blast radius
of changing two base classes that currently have no data members at all.

### 21.9 #2108 landed — the compatible half of #2098

Implemented exactly as §21.6 scoped it, with **zero object-layout change** (`sizeof` stays 40)
and therefore no approval. Six members gained the existing closed check, `SetLength`'s check
order was corrected to match `Read`/`Write`, and the three members that were already correct were
routed through the same `EnsureNotClosed()` so they cannot drift apart.

**+9 permanent regressions** (`SharpRuntimeTests_IO` 626 → **635**), including **#2098's layout
pin, landed early**. Landing the pin while #2098 is still blocked is what makes Approval IO-1
auditable: the approval sentence quotes `StringWriter` 384 → 392 and claims the other three are
free, and the pin proves the "before" half of that claim.

**The layout pin was itself mutation-checked**, which matters more than usual because it is
evidence for an unapproved decision. Adding a `bool` to the `TextWriter` **base** — option (b) —
made it fail with exactly the numbers §21.3 predicts: `TextWriter` **8 → 16**, `StreamWriter`
**24 → 32**, `StringWriter` **384 → 392**. Option (b)'s blast radius is therefore not an
estimate in this document; it is a measured, reproducible test failure.

**Three implementation mutations**, all against the final source, control clean, restores
byte-identical: the guard neutered → **5** tests; `SetLength`'s original check order → **1**; the
raw-pointer getter unguarded → **2**.

**ASan + UBSan + LSan clean over 22,000 rejections and 22,000 acceptances**, control proven live,
`UnmanagedMemoryStream.cpp` compiled into the instrumented translation unit.

**#2098 is now `blocked`**, scoped to SR-AUD-337 and SR-AUD-343 only, pending Approval IO-1.


---

### 20.6 #2099 landed, and the surviving half was SIX members with two wrong premises

**Ticket #2099 (SR-AUD-342 surviving half, cause I-A) — `todo` → `done`, 2026-08-12.
SR-AUD-342 `confirmed` → `remediated`.**

#### The dependency that did not apply

§9 sequenced #2099 "AFTER #2098" so it could inherit the storage answer. **There is no storage
question for `FileStream`**: `file_.is_open()` already is the closed state, `Close()` already
clears it, and `getCanRead`/`getCanWrite`/`getCanSeek` already consult it. `sizeof(FileStream)`
is **576** and `alignof` **8**, before and after. So #2099 is layout-neutral, needs no approval,
and split out on exactly the reasoning #2108 used for `UnmanagedMemoryStream`. #2098 keeps its
four text wrappers and stays blocked on Approval IO-1 — nothing here consumed or pre-empted it.

The ticket's own note anticipated this ("VERIFY FIRST whether FileStream already has a closed
flag — if so this is layout-neutral"). It does; it is.

#### What §6.2 got wrong

§6.2 scoped the surviving half to three members and said `Seek()` "succeeds outright". Measured
(`build-probe/2099_probe1_before.log`), it is **six** members, and `Seek` is not uniform:

| Member | Before | §6.2 predicted |
|---|---|---|
| `getLengthProperty()` | returned `5` — re-`stat`s `path_` | yes ("stale 5" — actually *live*, from the path) |
| `getPositionProperty()` | returned `-1` | yes |
| `setPositionProperty(0)` | succeeded | no |
| `Seek(0, Begin)` / `Seek(0, End)` | succeeded | yes |
| `Seek(0, Current)` | **threw `IOException`** | **no — §6.2 says it succeeds** |
| `Flush()` | succeeded silently | no |
| `setPositionProperty(-1)`, `SetLength(-1)` | argument checked **before** closed state | no |

1. **`Seek(0, Current)` reported the wrong *diagnostic*, not merely the wrong outcome.** The `-1`
   from `getPositionProperty()` does not stay local — `Stream::Seek` adds it to the requested
   offset and rejects the negative sum — so a **closed** stream complained *about the seek
   target*: `IOException("An attempt was made to move the position before the beginning of the
   stream.")`. A bare `EXPECT_THROW` would have passed against the old code. Only the exception
   type discriminates, which is why the pin asserts the type.
2. **Two members tested their argument first**, the same reversal #2108 found in
   `UnmanagedMemoryStream::SetLength`. `FileStream` had it in *two* places.

Also re-measured and recorded: `getLengthProperty()` was not returning a *cached* stale value —
it answers from `std::filesystem::file_size(path_)`, so a closed stream reported the file's
**current** length. The observable is the same; the mechanism is not what §6.2 implies.

#### The repair

One private `EnsureNotClosed()`, routed through by every dependent member **including the four
that were already correct** (`Read`, `Write`, `WriteByte`, `SetLength`), so they cannot drift
apart again. The message is the one those four already raised, unchanged
(`ObjectDisposedException("Cannot access a closed file.")`). `Seek` is not overridden: it is
inherited from `Stream` and expressed in terms of the position and length members, so all three
origins now report the closed file through them.

#### The residue, pinned rather than removed

On a **closed** stream, `Seek(-1, Begin)` still reports `IOException`, because `Stream::Seek`
tests a negative *resulting* position before delegating. Measured
(`build-probe/2099_probe2_seek_residue.log`), **`UnmanagedMemoryStream` does exactly the same**
after #2108. This is a property of the shared base algorithm, not a `FileStream` defect;
overriding `Seek` in `FileStream` alone would create a divergence between two siblings that
agree today. It is pinned by test. Whether `Stream::Seek` should check disposal before the
seek-target rule is a separate question for the whole hierarchy and is **not ticketed here**.

#### Gates

`SharpRuntimeTests_IO` **635 → 644, +9, add-only**; no pre-existing assertion edited or deleted.
Five implementation mutations, each rebuilt and relinked, discriminating **4 / 1 / 4 / 3 / 1**
distinct tests, with a clean control and a byte-identical restore verified by `sha256sum`:

| Mutation | Caught by |
|---|---|
| M1 `getPositionProperty` guard removed (restores the `-1` sentinel) | 4 |
| M2 `setPositionProperty` argument-before-closed order restored | 1 |
| M3 `Flush` silently no-ops again | 4 |
| M4 `getLengthProperty` guard removed | 3 |
| M5 `SetLength` argument-before-closed order restored | 1 |

**ASan + UBSan + LSan clean** with `FileStream.cpp` and `Stream.cpp` compiled from source into the
instrumented translation unit — justified here because the repair adds unwind paths through
members that previously returned normally, not run ceremonially. **Descriptor delta 0** across 100
closed-stream rejection cycles, *reported* rather than asserted as the acceptance criteria asks
(the test also asserts equality, which is strictly stronger and costs nothing).

No signature, object-layout, vtable, mangled-symbol or `noexcept` change. The open-stream control
block of the probe is **byte-identical** before and after.

**Still open in this namespace after #2099:** #2098 (blocked, Approval IO-1), #2102 (todo),
#2104 (todo, LAST), #2105 and #2106 (deferred verification, both need the absent reference tree).
