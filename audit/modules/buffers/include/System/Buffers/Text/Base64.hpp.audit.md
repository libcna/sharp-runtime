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
