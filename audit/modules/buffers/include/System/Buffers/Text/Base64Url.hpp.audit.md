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
