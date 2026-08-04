# Audit: `modules/xml/src/System/Xml/XmlReader.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Xml`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Xml && build/SharpRuntimeTests_Xml --gtest_color=no` passed 377/377 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-348 — medium — XmlReader continues reading after Close

`Close()` only records the enum state.  `Read()` has no closed-state guard and resumes parsing: the direct probe prints `reader-after-close-read=1` and a non-closed state value after closing a reader for `<r><x/></r>`.  The lifecycle boundary is therefore not observable and can consume input through a closed public reader.

## Remediation record — #2078 (2026-08-04)

**REMEDIATED.** The finding's evidence is retained above; this section appends what
implementation measured. Plan: `docs/SystemXmlNamespaceReviewPlan.md` §4.4 and §20.5.
Reproduction: `build-probe/2078_probe1_reader_close.cpp`, logs
`build-probe/2078_probe1_before.log` → `build-probe/2078_probe1_after.log`.

**Worse and wider than filed.** `Read()` had no guard **and assigned
`ReadState::Interactive` on its way out**, so the closed state did not survive a single
call — `Read(); Close(); Read()` reported `Interactive` on the *next* node:

| Member, after `Close()` | Before | After |
|---|---|---|
| `Read()` | `true`, cursor advanced, state → `Interactive` | `false`, cursor parked, state stays `Closed` |
| `ReadStartElement()` | advanced, state → `Interactive` | `XmlException`, state stays `Closed` |
| `ReadEndElement()` | advanced through `Read()` | `XmlException`, state stays `Closed` |
| `ReadElementContentAsString()` | returned `"t"`, advanced, state → `Interactive` | `""`, state stays `Closed` |
| `MoveToElement()` | `true` | `false` |
| `MoveToNextAttribute()` | `true`, then reported `a="1"` | `false` |
| `GetAttribute("a")` | `"1"` | `""` |
| `getNodeType/Name/Value/IsEmptyElement` | still reported the last node | `None` / `""` / `""` / `false` |

**The corrected premise is what made the repair small.** This class **already implements one
terminal read state correctly**: measured on the same document, a reader driven past its last
event reports `EndOfFile`, returns `false` from every further `Read()`, and never leaves that
state. `Closed` is now the same shape. Consequences:

- **No new state field.** `ReadState::Closed` *is* the flag — `Close()` is its only writer —
  so there is no layout change even in the opaque `XmlReaderState`.
- **No invented values.** Every accessor routes the closed case into the early return it
  already had for "there is no current node".
- **No new exception identity.** `ReadStartElement`/`ReadEndElement` throw the same
  `XmlException` they already threw for any other wrong position, because a closed reader is
  on no node.

The `return false` behaviour itself is **this port's choice**, selected because it is
self-consistent with the port's own `getReadStateProperty()` contract and with `EndOfFile`
one function away — **not** because the reference was consulted (`/rv/tmp/runtime/` absent).

**+12 permanent regressions, add-only — not one existing test was updated.**
`SharpRuntimeTests_Xml` 427 → 439.

**Three mutations, reverted from an exact backup with `git diff --stat` identical on both
sides** (build output unsuppressed after #2076's stale-binary lesson): R1 (`Read()` unguarded
again) fails exactly 5 tests; R2 (accessors/navigation ignore the closed state) 7; R3
(`isClosed` always false) 10. The two **control** tests — `EndOfFile` unchanged, and reading
unaffected until `Close()` is called — fail under **none** of them, which is what proves the
narrowing is confined to the closed state.

**Sanitizers: not applicable, with the reason stated rather than assumed.** §13 of the plan
predicts none for this ticket and the repair supports that: it adds no allocation, no
arithmetic and no ownership change, and every guard strictly *shrinks* the set of reachable
event indices. The `Xml` bodies were nevertheless exercised under ASan/UBSan/LSan in the same
batch (`build-probe/2076_probe4_asan.log`, and #2079's run) with the module compiled from
source.

**Cause X-D is shared with `modules/io`.** SR-AUD-337, SR-AUD-343 and SR-AUD-344 are the same
sentence about `StreamReader`, `StringWriter`/`StringReader` and `UnmanagedMemoryStream`.
**CCF-022 is still NOT minted** — the plan's §17 rule is to mint it when `modules/io` is
reviewed, citing all five, and this ticket does not change that.

## Missing assertions and diagnostics

- The focused suite does not call `Read()` after `Close()`, assert a closed-state lifecycle failure, or verify that parsing position remains unchanged.
- Add a state-transition diagnostic before a closed reader can traverse native XML input.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
