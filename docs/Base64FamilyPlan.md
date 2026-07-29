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
| SR-AUD-079 | — | both headers' `decodeCore` and `validateCore` | The unused low 2 bits (one `=`) / low 4 bits (two `=`) of the final quantum are never required to be zero, so `AB==`, `AAB=`, `AB`, `AAB` decode and validate | `Base64Helper/Base64DecoderHelper.cs`, `Base64ValidatorHelper.cs` |
| SR-AUD-080 | — | `Base64::decodeCore` | A padded quantum decodes to `Done` even when `isFinalBlock == false` | `Base64DecoderHelper.cs`, `skipLastChunk = isFinalBlock ? 4 : 0` |
| SR-AUD-081 | — | `Base64::decodeCore` | After a padded quantum, trailing whitespace is added to `bytesConsumed`; .NET leaves it for the enclosing parser | current .NET Base64 decoder test base |
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
| **#1817** | SR-AUD-079 — canonical final-bit validation, both headers, decode **and** validate | #1815 | **narrowing**: input accepted today becomes `InvalidData` | `todo` |
| **#1818** | SR-AUD-080 — padding is invalid while `isFinalBlock == false` | #1815, and should follow #1817 (same `decodeCore` final-quantum branch) | **narrowing** | `todo` |
| **#1819** | SR-AUD-081 — trailing whitespace after a padded quantum is not consumed | #1815, and should follow #1818 (same cursor code) | changes `bytesConsumed` only | `todo` |
| **#1820** | SR-AUD-082 — accept optional final `=`/`%` in Base64Url decode/validate | #1815; independent of #1817–#1819 | **widening**: only adds accepted input | `todo` |

**Why #1817–#1819 are ordered rather than parallel.** All three edit the same
final-quantum branch of one `decodeCore`. Taken in parallel they would conflict
line-for-line and each would have to re-derive the same padding state machine.
Taken in this order, each starts from a `decodeCore` whose final-quantum handling
is already one step closer to `Base64DecoderHelper.cs`.

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
