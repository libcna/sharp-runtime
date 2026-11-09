<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `BinaryReader` decodes as .NET Framework 4.0 (CNA HND-TQ-16)

*2026-11-09.* `BinaryReader` refused two inputs that .NET Framework 4.0's `BinaryReader` accepts:
a fifth `Read7BitEncodedInt` byte above `0x0F`, and invalid UTF-8 in a character read. Its
`ReadString` copied the bytes verbatim, invalid ones included. The CNA project owner chose, on
2026-11-09, to change the shared runtime rather than add a content-only route, because Microsoft
XNA 4.0 content is read by .NET Framework 4.0's reader (CNA `handoff.md`, "Owner answers of
2026-11-09"). This is a deliberate .NET 4.0 target for this one type, not the .NET 11 snapshot
the rest of the runtime follows; where the two differ it is said below.

The source of record is .NET Framework 4.0's own `mscorlib.dll`, file version 4.0.30319.1,
SHA256 `5634668d4775b0113f08ea31093b281fea69bfc4e99227f5ca761b4ed98acc63`, read as IL. Every
behaviour below cites its method and IL offsets there, in the code comments and in
`modules/io/tests/System/IO/BinaryReaderNet4DecodingTests.cpp`.

No public signature, `noexcept`, vtable or base changes. The two private members
`hasPendingCharacter_`/`pendingCharacter_` became one `std::int32_t decoderBits_` in the same
slot; `sizeof(BinaryReader)` and `alignof` are unchanged (24/8 on LP64), pinned by
`BinaryReaderNet4DecodingTest.TheDecoderStateKeepsTheReaderLayout`. The private helper
`TryReadChar` was replaced by `DecodeUtf8`, `InternalReadOneChar` and `InternalReadChars`.
Consumers must rebuild against the new header, as for any private-member change.

---

## 1. `Read7BitEncodedInt`

`IL_0019`-`002b`: `count |= (b & 0x7F) << (shift & 31)` into an `Int32`, so a fifth byte keeps only
its low four bits. Only a fifth byte that still sets the continuation bit is refused, with
`FormatException`, before a sixth byte is read (`IL_0004`-`0018`).

| Bytes | Was | Is |
|---|---|---|
| `81 80 80 80 70` | `FormatException` | `1` |
| `FF FF FF FF 7F` | `FormatException` | `-1` |
| `80 80 80 80 80 00` | `FormatException` | `FormatException`, five bytes consumed |

.NET 11 refuses a fifth byte above `0x0F` (`BinaryReader.cs`, "Too many bytes"); .NET 4.0 does not.

## 2. Character and string decoding

`BinaryReader(Stream)` uses `new UTF8Encoding()` (`.ctor` `IL_0002`), whose decoder fallback is
`DecoderReplacementFallback("�")` (`UTF8Encoding.SetDefaultFallbacks` `IL_001f`-`003a`). One
decoder serves every character read and is always called with `flush = false`
(`DecoderNLS.GetChars` `IL_0054`-`005e`).

- **One U+FFFD per invalid sequence, as .NET 4.0 groups it** (`UTF8Encoding.GetChars`
  `IL_0000`-`05ea`). A lead byte that cannot start a sequence (`80`-`BF`, `C0`, `C1`, `F5`-`FF`) is
  one sequence. A pending sequence ended by a byte that is not a continuation is one sequence, and
  that byte is read again. A second byte that makes a sequence non-shortest, an encoded surrogate or
  beyond U+10FFFF is consumed into the one U+FFFD; the bytes after it are then separate
  continuations. So `E0 80 80` is two U+FFFD, where .NET 11's maximal-subpart rule writes three.
- **An unfinished sequence is not an error.** It stays in the decoder and continues with the next
  character or string read. `ReadChar` at the end of the stream throws `EndOfStreamException` as
  before; `ReadChars` and `Read(char[], …)` return what they have.
- **`ReadString`** decodes its bytes 128 at a time and returns the UTF-16 result as UTF-8:
  well-formed UTF-8 is byte-identical, each U+FFFD is `EF BF BD`. Its early refusal of a declared
  length longer than a seekable stream's remainder is gone; .NET 4.0 reads what is there and then
  throws `EndOfStreamException`, and so does this port. The result grows with the bytes read, so a
  corrupt length still cannot request a large allocation.
- **`ReadChar` has a one-character buffer** (`InternalReadOneChar` `IL_0045`-`00b9`). A scalar above
  the Basic Multilingual Plane, or a U+FFFD that completes together with the character of the byte
  that ended its sequence (`C3 41`), does not fit, and `ReadChar` throws `ArgumentException`
  ("The output char buffer is too small …", parameter `chars`, `Encoding.ThrowCharsOverflow`
  `IL_0000`-`0033`) after seeking a seekable stream back to where the call began
  (`IL_00c1`-`00e9`). The earlier port returned the two surrogates across two calls; neither .NET
  4.0 nor .NET 11 does. `ReadChars(1)` and `Read(buffer, i, 1)` throw the same way, without
  seeking back (`InternalReadChars` has no handler).
- **`PeekChar`** restores the stream position but not the decoder (`IL_001c`-`003c`), so peeking
  at an unfinished sequence leaves it pending. A non-seekable stream keeps this port's documented
  `NotSupportedException` (`docs/StreamCapabilityContractDesign.md`); .NET answers `-1` there.

| Call | Bytes | Was | Is |
|---|---|---|---|
| `ReadChar` | `FF` | `FormatException` | U+FFFD |
| `ReadChar` | `C3 00` | `FormatException` | `ArgumentException`, position restored |
| `ReadChar` | `F0 9F 98 80` | U+D83D, then U+DE00 | `ArgumentException`, position restored |
| `ReadChars(2)` | `41 C3` | `EndOfStreamException` | `['A']` |
| `ReadString` | `41 FF` | `41 FF` | `41 EF BF BD` |
| `PeekChar` | `C3` (at end) | `EndOfStreamException` | `-1` |

## 3. To migrate

Code that reads well-formed UTF-8 and 7-bit integers that fit in 32 bits sees no change, except
`ReadChar` on a supplementary character, which now throws as .NET does: read such text with
`ReadChars(n)` for `n >= 2`, or with `ReadString`.

Downstream measurement (SA-2 condition 5): in CNA, `ReadChar`/`ReadChars`/`PeekChar` are called by
the content readers for `char` values and the SpriteFont character map (UTF-16 code units XNA's
writer emits one at a time, never a surrogate pair together), and `ReadString` by every string
read; CNA's content tests pin the new behaviour at `ContentManager::Load<T>`
(`ContentNet4BinaryDecodingTest`). mobile-eggbert was searched for `ReadChar`, `ReadChars`,
`PeekChar` and `Read7BitEncodedInt`; the result is recorded in the CNA HND-TQ-16 report.
