# Audit: `modules/buffers/include/System/Buffers/Text/Base64Url.hpp`

## Metadata

- Audit status: AUDITED (540-line public header-only implementation, fully
  read).
- Validation: `Base64UrlTest.*` passed 31/31 in `SharpRuntimeTests_Buffers` on
  2026-07-26.
- Reproduction: `/tmp/sharp-runtimervc-base64url-audit-probe.cpp` was compiled
  with `g++ -std=c++20 -I modules/buffers/include -I modules/core/include
  /tmp/sharp-runtimervc-base64url-audit-probe.cpp build/libsharp_runtime_core.a
  -o /tmp/sharp-runtimervc-base64url-audit-probe` and executed on 2026-07-26.
- Reference: local .NET `Base64Url/Base64UrlEncoder.cs`, `Base64UrlDecoder.cs`,
  `Base64UrlValidator.cs`, the shared `Base64DecoderHelper.cs`, and Base64Url
  decoder/validation test sources were reviewed.

## Assessment

The unpadded ordinary encode/decode paths, URL alphabet, invalid length one,
straightforward whitespace, throwing conveniences, explicit maximum, and one
non-final incomplete quantum pass the direct suite.  This header duplicates
the same in-place write order and final-sextet validation omissions found in
the sibling Base64 header.  It also calls its no-padding output convention an
input rule, thereby rejecting two optional final padding forms that the current
.NET API deliberately supports.

## Finding references

- **SR-AUD-078 (extended):** `TryEncodeToUtf8InPlace` repeats the same
  full-triple-first loop at lines 257–264 before reading the remainder at lines
  267–280.  The probe encodes `{ 'A', 'B', 'C', 0 }` in an eight-byte buffer
  and prints `1,6,QUJDRA`; expected unpadded output is `QUJDAA`.  The write has
  replaced the unread fourth source byte and silently changed it to `D`.
- **SR-AUD-079 (extended):** final remainder decoding at lines 80–91 and
  `validateCore` at lines 99–115 do not require unused low bits to be zero.
  The probe reports `AB,0,2,1,0,1` and `AAB,0,3,2,0,1`
  (`value,status,consumed,written,firstOutput,isValid`), although .NET
  `Base64UrlValidator.cs` rejects the corresponding noncanonical final
  sextets.  This is the unpadded Base64Url instance of the same canonical
  decoding boundary.

## SR-AUD-082 — medium — Base64Url rejects optional valid final padding accepted by .NET

The C++ decoding table assigns `-1` to `=` and `%` (lines 30–47), and
`decodeCore` immediately returns `InvalidData` for either (line 70).  Thus
`IsValid` rejects them too.  Current .NET's Base64Url output omits padding but
its decoder and validator intentionally support optional standard `=` and URL
`%` final padding: `Base64UrlDecoderByte.IsValidPadding` accepts both, and the
current Base64Url decoder tests state that one or two final padding characters
are valid when `isFinalBlock` is true.

The probe yields `QQ==,3,0,0,0,0` and `QQ%%,3,0,0,0,0` (`3` is
`OperationStatus::InvalidData`), while current .NET accepts both as a final
encoding of `A`.  The public header advertises itself as a .NET counterpart
and describes only how it *encodes* without padding; it does not document a
stricter decode adaptation.  This prevents C++ consumers from reading valid
current-.NET Base64Url input.

## Other missing assertions and diagnostics

- In-place tests cover only exactly three bytes.  They omit 4/5 bytes, longer
  non-multiples of three, zero remainder after a preceding group, and sentinel
  checks that prove a remainder is read before an output write can reach it.
- Invalid coverage omits optional `=`/`%` final padding, invalid placement or
  count of those padding characters, padding split by whitespace, and both
  `char` and UTF-8 overloads for every grammar boundary.
- No direct test rejects nonzero unused bits for remainders two and three, or
  asserts that `Decode*` and `IsValid` agree on that canonical rule.
- The one non-final case covers a simple unpadded remainder only.  It omits
  the current .NET padded-input rejection path, prior full output, destination
  exhaustion, and pre-populated `consumed`/`written` output diagnostics.
- Short destination tests do not record partial cursors, output nonmutation,
  or the distinct final unpadded remainder sizes.  There is no generated or
  differential corpus against local .NET Base64Url vectors.
- `invalidDataMessage()` refers to ordinary Base64 and padding count despite a
  Base64Url surface.  The missing test diagnostics leave this public error
  taxonomy/wording mismatch unobserved.

## Final assessment

The direct suite is green but does not expose the repeated in-place corruption,
the noncanonical decoder acceptance, or the unsupported optional input
padding.  No production or test source was modified during this audit.

## Post-audit remediation for SR-AUD-078 / CCF-013 (ticket #1816, 2026-07-29): REMEDIATED

The audit evidence above is retained unchanged. **SR-AUD-079 (extended) and SR-AUD-082 in this file are
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

The audit evidence above is retained unchanged. **SR-AUD-082 in this file, ticket #1820, stay `confirmed`.**

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
