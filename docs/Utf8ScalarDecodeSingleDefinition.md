<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# One UTF-8 scalar decode, in one place (ticket #2354)

*2026-08-18.* `System/detail/Utf8Scalar.hpp` is now the **only** definition of this runtime's
UTF-8 scalar decode. Six copies existed across four modules; #2014 moved two, and #2354 moved the
remaining four.

**No behaviour changes.** Every door produces byte-identical output for every input. There is no
signature change, no layout change and nothing to migrate; this note is a design record, not a
migration note.

---

## 1. Where the copies were

| Copy | Module | Named by the ticket? | Fate |
|---|---|---|---|
| `ASCIIEncoding.cpp` | `text` | — | moved by #2014 |
| `Latin1Encoding.cpp` | `text` | — | written against the shared rule by #2014 |
| `UnicodeEncoding.hpp` | `text` | ✅ | moved |
| `UTF32Encoding.hpp` | `text` | ✅ | moved |
| `Rune.hpp` | `text` | ✅ | moved |
| `Utf8JsonWriter.cpp` | `text-json` | ❌ | moved — byte-for-byte identical |
| `IdnMapping.cpp` | `globalization` | ❌ | moved |
| `UTF8Encoding.cpp` | `text` | ❌ | moved — see §3 |

**The ticket named three and there were six.** That is the finding, not a footnote: a rule
duplicated often enough that nobody could enumerate its copies is exactly the shape that produced
the defects #2014 and the IdnMapping repair were written to fix (`"\xC2\x41"` decoding to a
garbage code point, `"\xC0\x80"` decoding straight through to real U+0000).

## 2. Three contracts over one rule

The copies were not gratuitous. Three doors genuinely want three different things, and they
differ on exactly one input class: a **structurally valid** encoding of a value that is **not a
Unicode scalar** — a surrogate, or a value above `U+10FFFF`.

| Door | On ill-formed input | Consumes |
|---|---|---|
| `Rune::TryGetRuneAt` | reports `false` | the **sequence's own length** for a non-scalar, `1` for a structural break |
| any `Encoding` | substitutes `U+FFFD` | always `1`, so one replacement per byte, as .NET's replacement `DecoderFallback` does |
| `IdnMapping` | throws `ArgumentException` | — |

Collapsing that difference would have been a silent behaviour change in whichever door lost, so
it is a **parameter of the contract** rather than an accident of which copy you read:

```cpp
bool TryDecodeUtf8Scalar(const char* s, std::size_t size, std::size_t i,
                         std::uint32_t& codePoint, std::size_t& length);   // reports
void DecodeUtf8Scalar(const std::string& s, std::size_t i,
                      std::uint32_t& codePoint, std::size_t& length);      // substitutes
```

`DecodeUtf8Scalar` is defined as `if (!Try(...)) { codePoint = 0xFFFD; length = 1; }` — five
lines, and the substituting contract is now visible instead of being spread through five
`return` statements.

`IdnMapping`'s two hand-spelled extra rejections are subsumed exactly: a 2-byte lead below
`0xC2` is an overlong encoding, and a 4-byte lead above `0xF4` encodes a value above `U+10FFFF`.

## 3. The copy #2014 could not move

`UTF8Encoding.cpp`'s `wellFormedUtf8Length` reads a `(pointer, end)` range rather than a
`std::string`, and returns a validity **length** rather than a code point — its well-formed bytes
pass through unchanged, so there is no re-encoding step to fold a substitution into. That was a
real obstacle, and the answer is that the range is now a **parameter** of the shared decode
rather than a reason to keep a sixth copy. `size` is an exclusive bound, so a caller decoding a
sub-range gets a truncated sequence reported as ill-formed instead of read past.

## 4. The test that did not exist

`Rune::TryGetRuneAt` had **no test anywhere in the repository**, and a mutation collapsing its
length onto the `Encoding` contract passed all 17,296 tests then present. That gap is what makes
a refactor like this dangerous, and it is closed first:

* `modules/text/tests/System/Text/Utf8SharedScalarDecodeTests.cpp`
* `modules/globalization/tests/System/Globalization/IdnMappingUtf8DecodeTests.cpp`

Two files rather than one because `modules/text` does not depend on `Globalization`, and a
refactor is not a reason to add a public component edge. **The module graph is unchanged at
41 modules / 92 edges.**

| Mutation | Caught |
|---|---|
| Collapse the two contracts (a non-scalar reports length 1) | ✅ — **only after** the new pin; nothing caught it before |
| The substituting form propagates the `Try` length | ✅ |
| Drop the 3-byte branch's overlong rejection | ✅ — **only after** adding per-branch overlong rows; the 2-byte row alone missed it |
| Drop the surrogate rejection | ✅ (three pre-existing suites) |
| `UTF8Encoding` returns a length even on failure | ✅ |

Two of the five needed a new assertion. Both are recorded here rather than quietly fixed, because
they are the measurement of how much of this rule was untested while it was duplicated six ways.

## 5. Net effect

−179 lines. One definition. `System::Text::detail` still re-exports both entry points, so every
caller written against the old spelling is unchanged.
