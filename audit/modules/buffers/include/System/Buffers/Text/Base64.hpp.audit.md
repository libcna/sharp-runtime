# Audit: `modules/buffers/include/System/Buffers/Text/Base64.hpp`

## Metadata

- Audit status: AUDITED (621-line public header-only implementation, fully
  read).
- Validation: `Base64Test.*` passed 40/40 in `SharpRuntimeTests_Buffers` on
  2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-base64-audit-probe.cpp` was compiled
  with `g++ -std=c++20 -I modules/buffers/include -I modules/core/include
  /tmp/sharp-runtimervc-base64-audit-probe.cpp build/libsharp_runtime_core.a
  -o /tmp/sharp-runtimervc-base64-audit-probe` and executed on 2026-07-26.
- Reference: local .NET `Base64Encoder.cs`, `Base64Decoder.cs`,
  `Base64Validator.cs`, `Base64Helper/Base64DecoderHelper.cs`,
  `Base64Helper/Base64ValidatorHelper.cs`, and the Base64 decoder/validation
  unit suites were reviewed.

## Assessment

The ordinary byte and `char` encoding/decoding paths, normal padding, ASCII
whitespace, simple short destinations, explicit size boundaries, and the
incomplete non-final quantum pass the direct suite.  The compact handwritten
decoder has however lost several details enforced by the current .NET helper:
canonical final-bit validation, padding prohibition for an incomplete streaming
call, and exact cursor reporting after a padded quantum.  Its in-place encoder
also has an independently severe write-order error for input lengths that have
both a full triple and a trailing remainder.

## SR-AUD-078 — high — EncodeToUtf8InPlace overwrites an unread trailing source byte

`EncodeToUtf8InPlace` writes full triples backwards (lines 332–339), then reads
the one- or two-byte trailing remainder (lines 341–355).  For `dataLength` 4 or
5, the full triple at index zero writes its fourth encoded byte at offset 3
before the code reads the original remainder at offset 3.  The claimed
last-to-first ordering is therefore insufficient: the remainder must be
preserved/encoded before that group, or the algorithm must use a safe adjusted
order.

The standalone probe initializes an eight-byte buffer with `{ 'A', 'B', 'C',
0 }` and calls `EncodeToUtf8InPlace(buffer, 4, written)`.  It reports:

```text
0,8,QUJDRA==
```

where `0` is `OperationStatus::Done`.  The expected RFC 4648 / .NET output is
`QUJDAA==`; the returned string instead decodes as `ABCD`, demonstrating data
corruption without an error status.  The same dependency exists for every
full-triple-plus-remainder length.  The direct tests cover exactly three bytes
and exactly two bytes, never 4, 5, or larger non-multiple-of-three in-place
input.

## SR-AUD-079 — medium — decoder and validator accept noncanonical padded Base64

`decodeCore` derives the final one/two-byte output from the first two/three
sextets (lines 94–102) but never tests the unused low four bits before `==` or
the unused low two bits before `=`.  `validateCore` repeats the omission (lines
146–153), so `IsValid` agrees with the invalid decode result rather than
screening it.

The probe records the following for `AB==` and `AAB=` respectively:

```text
4,0,4,1,0,1
4,0,4,2,0,1
```

Each line is `inputLength,status,consumed,written,firstOutput,isValid`; both
malformed representations return `Done` and `IsValid == true`.  Current .NET
explicitly rejects these forms: `Base64DecoderHelper.cs` tests the unused two
bits for one `=` and four bits for `==` (lines 203–229), and
`Base64ValidatorHelper.cs` applies the equivalent check before declaring a
sequence valid.  Accepting alternate spellings makes validation weaker and
breaks consumers that depend on a canonical encoded representation.

## SR-AUD-080 — medium — streaming decode accepts padding while `isFinalBlock` is false

The C++ implementation only consults `isFinalBlock` after an incomplete
un-padded group (lines 118–120).  A complete padded group is decoded and
returns `Done` regardless of the flag (lines 87–112).  Current .NET routes a
non-final call around final-padding handling; padding in that state is invalid,
so chunked callers cannot accidentally treat a terminal quantum as ordinary
intermediate data.

The final probe line invokes `DecodeFromUtf8("QQ==", ..., false)` and reports
`4,0,4,1,65,1`: `Done`, four consumed bytes, and decoded `A`.  The matching
.NET decoder's `skipLastChunk = isFinalBlock ? 4 : 0` path feeds `=` through
ordinary decoding and returns `InvalidData`.  This divergence applies to both
UTF-8 and `char` overloads through the shared core; the current test only
checks an unpadded incomplete group with `isFinalBlock == false`.

## SR-AUD-081 — medium — padded decode incorrectly consumes trailing whitespace

After a padded quantum, `decodeCore` validates trailing whitespace but then
assigns `consumed = srcLen` (lines 106–112).  The current .NET Base64 test base
specifies that whitespace after end/padding is not included in consumed bytes;
this cursor is intentionally left for the surrounding parser/streaming loop.

For `"QQ== \\n"`, the probe reports `6,0,6,1,65,1`: success but six consumed
bytes.  The current .NET expectation is four consumed bytes (the padded
quantum), with the trailing whitespace remaining.  This makes a C++ caller
advance farther than its .NET counterpart and masks the boundary between a
Base64 field and following syntax.

## Other missing assertions and diagnostics

- No direct test uses an in-place source length of 4, 5, 7, or a randomized
  non-multiple-of-three length; it also omits sentinel bytes immediately after
  the logical input to prove no unread source is overwritten.
- There is no assertion that `AB==`, `AAB=`, and their whitespace-separated
  variants are rejected by `DecodeFromUtf8`, `DecodeFromChars`, both `IsValid`
  overload families, throwing wrappers, and the in-place decoder.
- The single `isFinalBlock == false` decoder case covers only an unpadded
  incomplete quantum.  It omits padded quantum rejection, a padded quantum
  split by whitespace, and cursor/output values after a preceding full group.
- Padding-followed whitespace has no consumed-count assertion; the suite only
  checks the decoded payload of embedded whitespace.
- Destination-too-small tests do not distinguish a failure before any quantum,
  after one full quantum, or at a padded final quantum.  The test suite also
  omits pre-populated output/cursor diagnostics on every non-`Done` status.
- `char` is a narrow C++ character rather than .NET UTF-16 `char`; the public
  comments limit it to ASCII Base64, but no compile-time/API documentation
  test records this intentional representation adaptation.

## Final assessment

The ordinary direct cases are green, but the in-place remainder overwrite is a
confirmed data-corruption defect and the shared decoder has three confirmed
streaming/canonical-validation divergences.  No production or test source was
modified during this audit.

## Post-audit remediation for SR-AUD-078 / CCF-013 (ticket #1816, 2026-07-29): REMEDIATED

The audit evidence above is retained unchanged. **SR-AUD-079, SR-AUD-080 and SR-AUD-081 in this file are
untouched and stay `confirmed`** — ticket #1816 repaired the in-place write order
only. The plan that scopes the rest of this cluster is
[`docs/Base64FamilyPlan.md`](../../../../../../../docs/Base64FamilyPlan.md)
(ticket #1815, design-only).

Ticket #1816 (`REMED-BUFFERS-BASE64-INPLACE-ORDER`, P1, size S) encodes the
trailing one/two-byte pack **before** the backwards loop over the full 3-byte
packs, in **both** `Base64::EncodeToUtf8InPlace` and
`Base64Url::TryEncodeToUtf8InPlace` — the two sites CCF-013 requires one repair to
cover. This is .NET's own order: `Base64Helper/Base64EncoderHelper.cs`'s
`EncodeToUtf8InPlace<TBase64Encoder>`, which its `Base64` and `Base64Url` in-place
encoders share, encodes the leftover pack first under the comment *"encode last
pack to avoid conditional in the main loop"*.

**The ordering argument, once.** Encoding pack `i` reads source `3i..3i+2` and
writes output `4i..4i+3`; since `4i >= 3i`, a pack can only overwrite source bytes
belonging to packs *after* it, so a last-to-first walk protects every full pack.
But the remainder is the last pack of all, and it was handled *after* the loop, so
the loop had already written over it.

**Measured before any production change** (`build-probe/1816_prefix_defects.cpp`,
logs `1816_prefix_defects.log` and `1816_postfix_defects.log`): a sweep of every
`dataLength` from 0 to 24 for both types, each in-place result compared against
*the same type's own out-of-place encoder*, with a sentinel byte immediately past
the encoded output.

| | Pre-fix | Post-fix |
|---|---|---|
| Cases wrong | **28 of 50** | **0 of 50** |
| Lengths affected, per type | 4, 5, 7, 8, 10, 11, 13, 14, 16, 17, 19, 20, 22, 23 — every length with both a full pack and a remainder | none |
| Status returned | `Done` / `true` in **all** 50 | unchanged |
| Sentinel past the output | never touched | never touched |

The finding named lengths 4 and 5 (`ABC\0` → `QUJDRA==` / `QUJDRA` instead of
`QUJDAA==` / `QUJDAA`); its own text adds that the dependency exists for every
full-triple-plus-remainder length, and the sweep is what makes that concrete. The
sentinel result matters too: this was silent corruption **inside** the declared
output, never an overrun, which is why no sanitizer had ever flagged it.

**Why the direct suite was green.** Before this ticket the in-place encoders were
tested at `dataLength` 3 (`Base64Test.EncodeToUtf8InPlace_RoundTrip`,
`Base64UrlTest.TryEncodeToUtf8InPlace_RoundTrip`) and 2
(`Base64Test.TryEncodeToUtf8InPlace_Success`), plus two short-destination cases.
Those are exactly the two shapes that **cannot** exhibit the defect — length 3 has
no remainder, length 2 has no full pack. Nothing covered a length with both.

Closure evidence: **8 new permanent regressions**, four per header — the audit's
own 4-byte reproduction, the 5-byte case, a 7-byte case proving the defect was
never limited to 4 and 5, and a 0..24 sweep that asserts equality with the
out-of-place encoder and an untouched sentinel at every length.
`SharpRuntimeTests_Buffers` is **473/473** (was 465), and the same 473 under
AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer with **zero
reports** (`build-asan/1816_buffers_asan.log`). Repository gate: 0 warnings, 0
errors, **14,002 tests across 37 executables** (was 13,994).

Source and ABI consequences: none. Both are `static` members of header-only
classes; no signature, layout or exported symbol changed. **Behavioural note for
consumers: any encoded output previously produced in place for a length with both
a full pack and a remainder was wrong and is now correct**, so a consumer that
stored or transmitted such output has stored corrupted data. No in-repository
caller uses these APIs outside their tests.

**One separate defect was found while planning and deliberately not folded in.**
.NET's helper short-circuits `if (buffer.IsEmpty) { bytesWritten = 0; return
OperationStatus.Done; }` *before* its destination-length check; this port has no
such short-circuit, so an empty buffer with a positive `dataLength` returns
`DestinationTooSmall`/`false` where .NET returns `Done`/`true`
(`build-probe/1815_empty_buffer_probe.log`). It carries **no `SR-AUD-*`
identifier** and is inactive ticket **#1821**, framed as a decision rather than a
foregone fix.

## Post-audit remediation for SR-AUD-079 (ticket #1817, 2026-07-29): REMEDIATED

The audit evidence above is retained unchanged. **SR-AUD-080 and SR-AUD-081 in this file, tickets #1818 and #1819, stay `confirmed`.**

Ticket #1817 (`REMED-BUFFERS-BASE64-CANONICAL-FINAL-BITS`, P2, size S) requires the
unused low bits of the final quantum to be zero, in **both** headers and in **both**
`decodeCore` and `validateCore`:

- a quantum carrying **one** byte (`XX==` padded, `XX` unpadded) uses only the top
  two bits of the second sextet, so its **low four bits** must be zero;
- a quantum carrying **two** bytes (`XXX=` padded, `XXX` unpadded) uses only the
  top four bits of the third sextet, so its **low two bits** must be zero.

.NET enforces exactly this: `Base64Helper/Base64DecoderHelper.cs` tests those bits
and `Base64ValidatorHelper.cs` applies the equivalent check before declaring a
sequence valid, as does `Base64UrlValidator.cs` for the unpadded form.

Measured before and after (`build-probe/1817_defects.cpp`, logs
`1817_prefix_defects.log` — built against the pre-fix headers — and
`1817_postfix_defects.log`), sixteen cases over both types, each recording the
decode status, `bytesWritten`, `IsValid`, and whether decoder and validator agree:

| Input | Type | Pre-fix | Post-fix |
|---|---|---|---|
| `AB==` | Base64 | `Done`, 1 byte, `IsValid` **true** | `InvalidData`, 0 bytes, `IsValid` false |
| `AAB=` | Base64 | `Done`, 2 bytes, `IsValid` **true** | `InvalidData`, 0 bytes, `IsValid` false |
| `AB` | Base64Url | `Done`, 1 byte, `IsValid` **true** | `InvalidData`, 0 bytes, `IsValid` false |
| `AAB` | Base64Url | `Done`, 2 bytes, `IsValid` **true** | `InvalidData`, 0 bytes, `IsValid` false |
| the 12 canonical spellings, both types | — | accepted | **unchanged** |

**The decoder and the validator agreed both before and after, and that is the
point.** A validator more permissive than its own decoder would be the worse
outcome — it tells a caller an input is safe to decode when it is not — so
`validateCore` had to gain the same rule in the same change. Base64Url's
`validateCore` only *counted* symbols and never kept their values; it now retains
the trailing sextets so it can apply the rule at all.

**Placement.** The canonical check runs **before** the destination-size check.
Whether an input is canonical is a property of the input alone and must not depend
on how much room the caller happened to provide; canonical input is unaffected
either way, so no existing `DestinationTooSmall` outcome changes.

**This narrows the accepted input set**, in the direction of .NET parity: input
that used to decode successfully is now `InvalidData`. A consumer that produced
noncanonical Base64 elsewhere and relied on this decoder accepting it will now see
a failure — which is the intent, since the previous behaviour silently discarded
bits the encoder never sets. Every one of the 104 pre-existing `Base64*` tests
still passes unmodified, so nothing in this repository depended on the old
acceptance.

Closure evidence: **12 new permanent regressions**, six per header — the
noncanonical one- and two-byte quanta rejected by the decoder, `IsValid` and its
`decodedLength` overload agreeing with the decoder, the `char` overloads inheriting
the rule through the shared core, six canonical spellings still decoding to the
same bytes, and a 0..24 round trip proving that everything this repository's own
encoder produces is still accepted. `SharpRuntimeTests_Buffers` is **485/485** (was
473), and the same 485 under AddressSanitizer + UndefinedBehaviorSanitizer +
LeakSanitizer with **zero reports** (`build-asan/1817_buffers_asan.log`).
Repository gate: 0 warnings, 0 errors, **14,014 tests across 37 executables** (was
14,002).

Source and ABI consequences: none. No signature, layout or exported symbol changed;
only the accepted input set did.

## Post-audit remediation for SR-AUD-080 (ticket #1818, 2026-07-29): REMEDIATED

The audit evidence above is retained unchanged. **SR-AUD-081 in this file, ticket
#1819, stays `confirmed` at the time of this note.**

Ticket #1818 (`REMED-BUFFERS-BASE64-NONFINAL-PADDING`, P2, size S) makes `'='`
`InvalidData` whenever `isFinalBlock` is false, in `Base64::decodeCore` — which both
the UTF-8 and the `char` overload share, so both inherit the rule.

**Why this is .NET's rule, from two independent paths.**
`Base64Helper/Base64DecoderHelper.cs`'s `DecodeFrom` sets
`int skipLastChunk = isFinalBlock ? 4 : 0`, so with the flag clear `maxSrcLength`
covers the *whole* source and every character — padding included — goes through the
four-element loop, where `'='` is unmapped (`-1`) and the call returns
`InvalidData`; the tail block that actually understands padding is unreachable, as
that file's own comment says (*"if isFinalBlock is false, we will never reach this
point"*). The whitespace-tolerant path agrees: `DecodeWithWhiteSpaceBlockwise`
computes a per-block `localIsFinalBlock` and then executes
`if (localIsFinalBlock && !isFinalBlock) localIsFinalBlock = false;`, so no block of
a non-final call is ever decoded as a final one.

**Placement.** The check fires at the **first** padding character, before the
quantum is completed and before any destination arithmetic. That is what leaves
`bytesConsumed`/`bytesWritten` on the last completed quantum boundary — where .NET
leaves them too — and it also means a destination too small to hold the quantum
cannot mask the rejection.

**Measured before and after** (`build-probe/1818_defects.cpp`, logs
`1818_prefix_defects.log` and `1818_postfix_defects.log`), each input run through
**both** the UTF-8 and the `char` overload, which agreed on every case:

| Input (`isFinalBlock == false`) | Pre-fix | Post-fix | Current .NET |
|---|---|---|---|
| `QQ==` | `Done`, 4 consumed, 1 written | `InvalidData`, 0, 0 | `InvalidData`, 0, 0 |
| `QUJDQQ==` | `Done`, 8, 4 | `InvalidData`, 4, 3 | `InvalidData`, 4, 3 |
| `QUJDQUI=` | `Done`, 8, 5 | `InvalidData`, 4, 3 | `InvalidData`, 4, 3 |
| `QQ==QUJD` | `InvalidData`, 4, 1 | `InvalidData`, 0, 0 | `InvalidData`, 0, 0 |
| `QQ =  = ` | `Done`, 8, 1 | `InvalidData`, 0, 0 | `InvalidData`, 0, 0 |
| `QUJD QQ==` | `Done`, 9, 4 | `InvalidData`, 4, 3 | `InvalidData`, **5**, 3 |
| `QUJD` (no padding) | `Done`, 4, 3 | unchanged | `Done`, 4, 3 |

**The finding understated the surface.** It named `DecodeFromUtf8("QQ==", …, false)`
and the bare padded quantum. The sweep shows the divergence also covered a padded
quantum *after* a complete one, the single-`=` spelling, padding in a non-terminal
position, and padding split by whitespace — six of the seven non-final shapes
probed, not one.

**Two residual divergences, both in the cursor reported alongside `InvalidData`,
are recorded rather than fixed.** `QUJD QQ==` differs by the one whitespace byte
.NET's `InvalidDataFallback` skips before re-entering the decoder, and the
pre-existing `QQ==QUJD` with `isFinalBlock == true` differs because
`DecodeWithWhiteSpaceBlockwise` *reverts* its block counters to `0,0` when
non-whitespace follows the padding, while this port reports the quantum it had
already accepted. Both are the same class — what `bytesConsumed`/`bytesWritten`
mean on a failed decode — neither changes a status or a decoded byte, and both are
inactive ticket **#1822** with no `SR-AUD-*` identifier (numbering stays frozen at
364).

**This narrows the accepted input set**, like #1817 and in the same direction:
padded text that used to decode with `isFinalBlock == false` is now `InvalidData`.
Every `isFinalBlock == true` outcome is byte-for-byte unchanged, and so is
`IsValid` — it has no `isFinalBlock` parameter and *is* the final-block decoder's
validator, so decoder/validator agreement is preserved without touching
`validateCore`. Unpadded incomplete quanta keep `NeedMoreData`: the ticket narrows
padding only, never the streaming contract the flag exists for. All 104 pre-existing
`Base64*` tests still pass unmodified.

Closure evidence: **7 new permanent regressions** — padded rejection with the exact
cursor on five shapes, the `char` overload inheriting it, every `isFinalBlock == true`
outcome pinned unchanged, unpadded quanta still `NeedMoreData`, the rejection
beating a zero-length destination, `IsValid` unaffected, and a two-chunk stream
proving the flag still does its job. `SharpRuntimeTests_Buffers` is **492/492** (was
485), and the same 492 under AddressSanitizer + UndefinedBehaviorSanitizer +
LeakSanitizer with **zero reports** (`build-asan/1818_buffers_asan.log`); the probe
itself is clean under the same three (`build-probe/1818_asan.log`). Repository gate:
0 warnings, 0 errors, **14,021 tests across 37 executables** (was 14,014).

Source and ABI consequences: none. No signature, virtual, return convention, object
layout or exported symbol changed; only the accepted input set did.
