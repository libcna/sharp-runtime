<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Base64 / Base64Url remediation family plan (CCF-013 and its neighbours)

Ticket **#1815** (`REMED-BUFFERS-BASE64-FAMILY-PLAN`, P2, size S, design-only).
No production source changed under this ticket.

This is the scoped plan the post-audit roadmap requires before touching a
cross-cutting family: *"first enumerate the exact listed `SR-AUD-*` members and
their current tests, then split work along public type/module boundaries"*
(`NEXT.md`, "Recommended dependency order", item 3). It exists so the
`modules/buffers` Base64 work is done as a planned family and not as scattered
file-by-file edits.

---

## 1. Why this family, and what it is not

`CCF-013 — sibling Base64 encoders duplicate an unsafe in-place write order` has
exactly **one** member finding, **SR-AUD-078**, and that finding spans **two**
public headers. The cause statement is explicit that a repair *"must cover both
headers and test every full-group-plus-remainder boundary rather than correcting
just the padded variant"*. That is the family's whole scope.

While enumerating it, four adjacent `confirmed` findings turned out to live in
the *same two headers* and to share the same shape — one algorithm duplicated
across a padded and an unpadded sibling, with the direct suite green because it
never covers the boundary. They are **not** CCF-013 members and are **not**
renamed into it, but planning them together is what stops the second, third and
fourth ticket from re-reading the same two files from scratch.

The distinction matters for scheduling: CCF-013 is a **correctness** repair (the
current output is wrong), while the four neighbours are **grammar/parity**
repairs (the current output is right for what it accepts, but the accepted set
differs from .NET). Correctness goes first.

---

## 2. Exact membership, current tests, and .NET reference

| Finding | Family | Sites | What is wrong today | .NET reference |
|---|---|---|---|---|
| **SR-AUD-078** | **CCF-013** | `Base64::EncodeToUtf8InPlace`, `Base64Url::TryEncodeToUtf8InPlace` | Full 3-byte packs are encoded before the trailing remainder is read, so the first full pack's fourth output byte overwrites it. Returns success. | `Base64Helper/Base64EncoderHelper.cs`, `EncodeToUtf8InPlace<TBase64Encoder>` — encodes the leftover pack **before** the backwards loop |
| SR-AUD-079 *(remediated, #1817)* | — | both headers' `decodeCore` and `validateCore` | The unused low 2 bits (one `=`) / low 4 bits (two `=`) of the final quantum are never required to be zero, so `AB==`, `AAB=`, `AB`, `AAB` decode and validate | `Base64Helper/Base64DecoderHelper.cs`, `Base64ValidatorHelper.cs` |
| SR-AUD-080 | — | `Base64::decodeCore` | A padded quantum decodes to `Done` even when `isFinalBlock == false` | `Base64DecoderHelper.cs`, `skipLastChunk = isFinalBlock ? 4 : 0` |
| SR-AUD-081 *(false positive, #1819)* | — | `Base64::decodeCore` | ~~After a padded quantum, trailing whitespace is added to `bytesConsumed`; .NET leaves it for the enclosing parser~~ — **inverted**: .NET counts it too | current .NET Base64 decoder test base — which says the opposite (see §8) |
| SR-AUD-082 | — | `Base64Url::decodeCore` and its table | `=` and `%` map to `-1`, so valid optional final padding is rejected | `Base64UrlDecoderByte.IsValidPadding` |

**Current direct test coverage of the in-place encoders, measured 2026-07-29**
(this is the reason the defect survived): `Base64Test` has
`EncodeToUtf8InPlace_RoundTrip` (dataLength **3**),
`EncodeToUtf8InPlace_TooSmall_ReturnsDestinationTooSmall` (dataLength 3, short
destination) and `TryEncodeToUtf8InPlace_Success` (dataLength **2**);
`Base64UrlTest` has `TryEncodeToUtf8InPlace_RoundTrip` (dataLength **3**) and
`TryEncodeToUtf8InPlace_TooSmall_ReturnsFalse`. Lengths 2 and 3 are precisely the
two shapes that **cannot** exhibit the defect: length 3 has no remainder, and
length 2 has no full pack. Nothing tested a length with both.

---

## 3. The defect and the fix, stated once

Encoding pack `i` reads source bytes `3i..3i+2` and writes output bytes
`4i..4i+3`. Since `4i >= 3i`, a pack can only overwrite source bytes belonging to
packs **after** it. Walking the full packs from last to first therefore protects
every full pack — but the **remainder is the last pack of all**, and it was
handled *after* the loop, so the loop had already written over it.

The fix is an ordering change, not a new algorithm: **encode the trailing pack
first, then walk the full packs backwards.** The remainder writes at
`4 * fullGroups`, which is at or after every source byte the full packs still
need, so nothing is clobbered in either direction.

This is what .NET does, for the same reason, in the helper both its `Base64` and
`Base64Url` in-place encoders share.

**Measured, before any change** (`build-probe/1816_prefix_defects.cpp`, log
`build-probe/1816_prefix_defects.log`): a sweep of every `dataLength` from 0 to
24, for both types, comparing the in-place result against *the same type's own
out-of-place encoder* and checking a sentinel byte immediately past the encoded
output. **28 of 50 cases were wrong** — every one of the 14 lengths per type that
has both a full pack and a remainder (4, 5, 7, 8, 10, 11, 13, 14, 16, 17, 19, 20,
22, 23). All 50 returned success. The sentinel was never touched, so this is
silent corruption *inside* the declared output, not an overrun. After the fix,
**0 of 50**.

The audit named lengths 4 and 5; its own text also says "the same dependency
exists for every full-triple-plus-remainder length", and the sweep is what makes
that concrete.

---

## 4. Ticket split, with dependencies

| Ticket | Scope | Depends on | Compatibility | Status |
|---|---|---|---|---|
| **#1815** | this plan | — | design-only | **done** |
| **#1816** | **SR-AUD-078 / CCF-013** — in-place write order, **both** headers | #1815 | compatible: fixes wrong output, no signature/ABI change | **done** |
| **#1817** | SR-AUD-079 — canonical final-bit validation, both headers, decode **and** validate | #1815 | **narrowing**: input accepted today becomes `InvalidData` | **done** |
| **#1818** | SR-AUD-080 — padding is invalid while `isFinalBlock == false` | #1815, and should follow #1817 (same `decodeCore` final-quantum branch) | **narrowing** | **done** |
| **#1819** | SR-AUD-081 — trailing whitespace after a padded quantum is not consumed | #1815, and should follow #1818 (same cursor code) | changes `bytesConsumed` only | **done — FALSE POSITIVE** |
| **#1820** | SR-AUD-082 — accept optional final `=`/`%` in Base64Url decode/validate | #1815; independent of #1817–#1819 | **widening**: only adds accepted input | **done** |

**Why #1817–#1819 are ordered rather than parallel.** All three edit the same
final-quantum branch of one `decodeCore`. Taken in parallel they would conflict
line-for-line and each would have to re-derive the same padding state machine.
Taken in this order, each starts from a `decodeCore` whose final-quantum handling
is already one step closer to `Base64DecoderHelper.cs`.

**#1818 landed on 2026-07-29**, after #1817 and in the order this table sets. It
turned out not to need the final-quantum branch at all: current .NET rejects
padding in a non-final call *before* any padding handling runs
(`skipLastChunk = isFinalBlock ? 4 : 0` in `Base64DecoderHelper.DecodeFrom`, and
`DecodeWithWhiteSpaceBlockwise` forcing its per-block `localIsFinalBlock` back to
false), so the repair is one rule — with `isFinalBlock` false, `'='` is invalid —
placed at the first padding character. The finding named one input; six of the
seven non-final shapes probed were wrong. Two residual cursor divergences on
`InvalidData` returns are ticket **#1822** (§10). `IsValid` needed no change:
having no `isFinalBlock` parameter, it *is* the final-block decoder's validator.

**#1817 landed on 2026-07-29** and is recorded in both Base64 audit reports. Two
things it settled that #1818 and #1819 inherit: the validator must change in the
**same** ticket as the decoder (a validator more permissive than its own decoder
tells a caller an input is safe to decode when it is not), and Base64Url's
`validateCore` now retains the trailing sextet values rather than only counting
symbols, which is the machinery a later ticket needs too.

**#1820 landed on 2026-07-29** and closed the last `confirmed` finding in
`Base64Url.hpp`. Two things it settled that this plan had assumed wrongly: the
decode *table* did not need to change — .NET recognises padding by a test on the raw
character, leaving `'='`/`'%'` unmapped, and copying that keeps the table a pure
sextet alphabet — and `invalidDataMessage()` did not need rewording, because .NET's
own `Base64Url` throws `SR.Format_BadBase64Char`, verbatim the string this port
already used. Its 62 vectors were all taken from named current-.NET tests:
**18 differed before, 0 after.**

**#1820 is deliberately unordered against them.** It touches the Base64Url
decode *table* and the early `-1` rejection, not the final-quantum branch, and it
moves in the opposite direction (accepting more, not less), so sequencing it
against the narrowing tickets would only delay it.

**None of the six needs the approval category** that
`ICollection::CopyTo`, `ReadOnlyDictionary::Empty` or the mutation-counter
widenings needed. No public signature, virtual, return convention, object layout
or iterator layout changes anywhere in this family; every ticket changes only
what a function computes. #1817 and #1818 do narrow the accepted input set, which
is a behavioural change a consumer can notice — but in the direction of .NET
parity, turning silent acceptance of malformed input into the documented
`InvalidData`, and each must say so explicitly in its own record.

---

## 5. One separate defect found while planning, not folded in

`Base64Helper.EncodeToUtf8InPlace` opens with

```csharp
if (buffer.IsEmpty) { bytesWritten = 0; return OperationStatus.Done; }
```

**before** its destination-length check. This port has no such short-circuit, so
an empty buffer with a positive `dataLength` takes the length check instead.
Measured (`build-probe/1815_empty_buffer_probe.log`):

| Input | This port | .NET |
|---|---|---|
| `EncodeToUtf8InPlace(empty, 0, w)` | `Done`, `w = 0` | `Done`, `w = 0` |
| `EncodeToUtf8InPlace(empty, 1, w)` | **`DestinationTooSmall`**, `w = 0` | `Done`, `w = 0` |
| `EncodeToUtf8InPlace(empty, 5, w)` | **`DestinationTooSmall`**, `w = 0` | `Done`, `w = 0` |
| `TryEncodeToUtf8InPlace(empty, 1, w)` (Url) | **`false`**, `w = 0` | `true`, `w = 0` |

This is a status divergence on a degenerate input, it carries **no `SR-AUD-*`
identifier** (the audit did not record it and the numbering is frozen at 364),
and it is tracked as inactive ticket **#1821** rather than widening #1816.
Whether to adopt .NET's short-circuit is a genuine question — reporting `Done`
for a request to encode five bytes into a zero-byte buffer is arguably the worse
contract — so #1821 is a decision, not a foregone fix.

---

## 6. What this plan does not cover

- `modules/buffers`' other `confirmed` findings — SR-AUD-075/085 (CCF-014,
  stale Try-output), SR-AUD-076, SR-AUD-077, SR-AUD-083, SR-AUD-084,
  SR-AUD-086, SR-AUD-087 — are in the same module but not in these two headers
  and not in this family. CCF-014 in particular is its own cause with its own
  membership and deserves its own plan.
- The decoders' `char` overloads share `decodeCore` with the UTF-8 ones, so
  #1817–#1820 each inherit both surfaces automatically; that is a property to
  **test**, not a separate ticket.

---

## 7. A second separate defect found while implementing #1818, not folded in

On an `InvalidData` return, `Base64::decodeCore` reports the cursor of the last
quantum it had already accepted. Current .NET sometimes reports a different one,
because it reaches `InvalidData` through a two-stage fallback rather than a single
pass. Two instances measured under #1818
(`build-probe/1818_defects.cpp`, log `1818_postfix_defects.log`):

| Input | `isFinalBlock` | This port | .NET | Why .NET differs |
|---|---|---|---|---|
| `QUJD QQ==` | false | `InvalidData`, 4, 3 | `InvalidData`, **5**, 3 | `InvalidDataFallback` skips the failed remainder's leading whitespace and adds it to `bytesConsumed` before re-entering the decoder |
| `QQ==QUJD` | true | `InvalidData`, 4, 1 | `InvalidData`, **0, 0** | `DecodeWithWhiteSpaceBlockwise` *reverts* its block counters when non-whitespace follows the padding |

The second instance predates #1818 entirely. Neither changes a returned status or
a decoded byte: the divergence is confined to the two out-parameters on a failed
decode, which .NET documents as a slicing aid rather than a contract.

This carries **no `SR-AUD-*` identifier** (the numbering stays frozen at 364) and
is tracked as inactive ticket **#1822** rather than widening #1818. Like #1821 it
is a decision, not a foregone fix: matching .NET exactly means reproducing its
two-stage structure — a fast path that fails, a whitespace-skipping re-entry, then
a block-wise decoder that can revert its own counters — which is a substantially
larger rewrite of `decodeCore` than the parity it buys.

---

## 8. SR-AUD-081 is a false positive (ticket #1819)

This plan's §2 repeated SR-AUD-081's premise — *"After a padded quantum, trailing
whitespace is added to `bytesConsumed`; .NET leaves it for the enclosing parser"* —
and cited "current .NET Base64 decoder test base" as the reference. **The premise is
inverted, and this plan carried it forward without checking.** That is recorded here
rather than by editing §2 into silence.

The test base says the opposite in three places:

- its member data is named
  `BasicDecodingWithExtraWhitespaceShouldBeCountedInConsumedBytes_MemberData` and
  yields `{ "AQ==" + whitespace(i), 4 + i, 1 }`;
- the same member data's second half yields `{ s+s+s+s, s.Length * 4, 12 }` for
  seven whitespace placements, one of them (`"MTIz "`) trailing;
- `DecodingWithWhiteSpaceSplitFinalQuantumAndIsFinalBlockFalse` asserts
  `bytesConsumed == base64Data.Length` for `"AQ\r\nQ=\r\n"`, and
  `DecodingWithEmbeddedWhiteSpaceIntoSmallDestination_TrailingWhiteSpacesAreConsumed`
  states it in its name.

For the finding's own `"QQ== \n"`, .NET reports **6** consumed, like this port:
`SrcLength` rounds the source to 4, `DecodeFrom` then fails
`if (srcLength != source.Length)` into `InvalidDataExit`, and `InvalidDataFallback`
finds the remainder to be all whitespace, adds `source.Length` and returns `Done`.

**Measured**: `build-probe/1819_defects.cpp` (log `1819_defects.log`) replays .NET's
own vectors with .NET's own expected values on both overloads. **27 of 27
whitespace-consumption vectors match.** No production source changed; four permanent
regressions now pin the behaviour.

The same run independently re-confirmed #1818 against .NET's own tests
(`"AAA="` → `InvalidData`, 0, 0; `"AAAA"` → `Done`, 4, 3; `"AQ\r\nQ="` →
`InvalidData`, 0, 0), which is a stronger check than the traced expectations #1818
was closed on.

**What it did find** is §7's ticket **#1822**, upgraded from two traced instances to
four .NET-test-pinned ones and from `InvalidData` only to `DestinationTooSmall` as
well. The rule .NET follows is uniform — *on a non-`Done` return the cursor advances
past whitespace to the first non-whitespace character at or after the last completed
quantum boundary* — with exactly one case outside it, `"QQ==QUJD"` with
`isFinalBlock` true, where `DecodeWithWhiteSpaceBlockwise` reverts its counters to
`0,0`. #1822 is now **P2**, and it landed the same day — see §10.

**Revised family status**: five tickets, of which #1815, #1816, #1817 and #1818 are
repairs or plans that landed, #1819 is a false positive, and #1820 remains. The
neighbours are therefore three parity repairs and one non-defect, not four parity
repairs.

---

## 9. Family status after #1820 (2026-07-29)

| Ticket | Finding | Outcome |
|---|---|---|
| #1815 | — | plan (this document) |
| #1816 | SR-AUD-078 / CCF-013 | **remediated** — in-place write order, both headers |
| #1817 | SR-AUD-079 | **remediated** — canonical final bits, both headers, decode and validate |
| #1818 | SR-AUD-080 | **remediated** — `'='` is `InvalidData` while `isFinalBlock` is false |
| #1819 | SR-AUD-081 | **false positive** — .NET counts the trailing whitespace too (§8) |
| #1820 | SR-AUD-082 | **remediated** — optional final `'='`/`'%'` accepted, decode and validate |

`CCF-013` is closed and every `SR-AUD-*` finding in `Base64.hpp` and `Base64Url.hpp`
is either `remediated` or corrected. The findings index records **21 remediated / 343
confirmed** of 364.

**What remains from this family are two decisions, not defects**, neither carrying an
`SR-AUD-*` identifier:

- **#1821** — the in-place encoders reject an empty buffer with a positive
  `dataLength` that .NET short-circuits to `Done` (§5). Adopting .NET's contract means
  reporting success for a request to encode five bytes into a zero-byte buffer.
- ~~**#1822** — the cursor reported alongside a non-`Done` status~~ — **landed on
  2026-07-29, see §10**, so #1821 is the only decision from this family still open.

**What this plan got wrong, recorded rather than edited away.** §2 repeated
SR-AUD-081's inverted premise unchecked (see §8). §4's table predicted that #1820
would change the Base64Url decode *table*; it did not, and should not have. Both are
the same failure mode: taking a finding's account of .NET at face value instead of
reading the current .NET source and its tests first. Every ticket in this family from
#1818 onward read the .NET tests before writing code, and that is what produced the
§7 and §8 discoveries.

---

## 10. The non-`Done` cursor is aligned, with one deliberate deviation (ticket #1822)

§7 opened #1822 as a decision. §8 turned it into a repair by finding that **four** of
its instances are pinned by named current-.NET tests rather than by tracing, and that
one of the four is a `DestinationTooSmall` return — so it was never an
`InvalidData`-only question. It landed on 2026-07-29.

**The rule**, now implemented in both `decodeCore`s: on an `InvalidData` or
`DestinationTooSmall` return, `bytesConsumed` is the first **non-whitespace** character
at or after the last completed quantum's boundary. That is where .NET's
`InvalidDataFallback` leaves it, having skipped the failing region's leading whitespace
and added it to the count before re-entering the decoder. Base64Url gets the same rule
because .NET's `DecodeFrom`/`InvalidDataFallback` is **one generic helper** shared by
both decoders.

**`NeedMoreData` is excluded.** .NET returns it from `NeedMoreDataExit`, which the
fallback never runs for, so its cursor stays on the quantum boundary. `"QUJD QQ"` with
`isFinalBlock` false is **4**, not 5. Applying the rule there would have introduced a
new divergence while fixing an old one — which is exactly the failure mode §9 records
this plan already committed twice.

**The deviation, decided explicitly.** `DecodeWithWhiteSpaceBlockwise` *reverts* its
block counters when non-whitespace follows a block's padding, so .NET reports `0,0`
for `"QQ==QUJD"` **while having already written the byte into the caller's
destination**. This port keeps reporting what it actually wrote. That behaviour is
pinned by none of .NET's own tests, and reporting fewer bytes written than were
physically written is the worse contract for a caller that inspects the buffer. Two
permanent regressions pin the deviation, including the invariant
`bytesWritten <= bytesConsumed`.

**Measured**: 41 vectors across both types, **9 wrong before, 0 after**
(`build-probe/1822_defects.cpp`). Re-running #1819's 27 vectors and #1820's 62 against
the new rule gives 0 differences each, so no previously verified cursor moved. No
status and no decoded byte changed anywhere, and every `Done` cursor is untouched.
