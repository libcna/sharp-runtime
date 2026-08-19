<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Regex::Escape`, `Regex::Split` and an unsuccessful `Match`'s `Index` (ticket #2397)

*2026-08-19.* Three public members of `System::Text::RegularExpressions` answered differently from
.NET. Two of them **lost data**. All three now match the reference exactly.

Landed under **SA-5** — aligning to the reference is ordinary work, including where emitted text
or returned content moves. There is **no layout, vtable, signature or `noexcept` change**: every
member keeps its declaration, and the module is a header-only `INTERFACE` target.

**Downstream, measured:** `System::Text::RegularExpressions` has **zero sites** in
`cna` and **zero** in `mobile-eggbert`. (The only `Regex` matches in `cna` are inside
`vendor/googletest`, which is a different library entirely.) First-party, the only affected test
was one shipped pin, which was **inverted rather than deleted**.

---

## 1. `Regex::Escape` — the escaped set and the escaped spelling are now .NET's

`RegexParser.cs:2135-2136` defines the metacharacters as
`SearchValues.Create("\t\n\f\r #$()*+.?[\\^{|")`. This port used its own set,
`"\\^$.|?*+()[]{}"`, which differed **in both directions**:

| | Characters | Effect |
|---|---|---|
| Missing here (6) | TAB, LF, FF, CR, SPACE, `#` | were emitted raw |
| Extra here (2) | `]`, `}` | were escaped where .NET leaves them bare |

`RegexParser.cs:180-199` additionally renders the four whitespace metacharacters as a backslash
followed by a **letter** — `\n` becomes `\` `n`, not `\` followed by the raw newline byte. This
port had no such rule because it had none of those characters in its set.

**Emitted text that moves:**

| Input | Before | After (= .NET) |
|---|---|---|
| `a b` | `a b` | `a\ b` |
| `a#b` | `a#b` | `a\#b` |
| `a` TAB `b` | `a` TAB `b` | `a\tb` (backslash, letter `t`) |
| `a]b` | `a\]b` | `a]b` |
| `a}b` | `a\}b` | `a}b` |
| `a{2}` | `a\{2\}` | `a\{2}` |
| `[a]` | `\[a\]` | `\[a]` |

**A vertical tab is still not escaped**, in either version, because `\v` is not in .NET's set —
the doc comment above `Escape` says "spaces" but the code names four specific whitespace
characters plus the space, and the code is what runs.

### 1.1 What does *not* change, and it was measured rather than assumed

`Escape` exists so that a literal can be used as a pattern. That property is unchanged:
**every one of the 127 single ASCII bytes, plus 18 composites, still compiles as a pattern under
this runtime's `std::regex` ECMAScript grammar and still matches exactly its own literal** — zero
`regex_error`, zero wrong matches. This mattered because both directions of the set change carried
a risk: the newly escaped forms (`\ `, `\#`, `\n`, `\r`, `\t`, `\f`) had to be *accepted* by
libstdc++, and the newly bare `]` and `}` had to be accepted as *literals*. All six and both are.

### 1.2 The one behavioural narrowing, and it is .NET's own

A caller who splices `Escape` output **into a character class** — `"[" + Escape(x) + "]"` — used
to be protected by the extra `\]`. They no longer are: `Escape("]")` is now `"]"`, so that
composition ends the class early. **.NET behaves identically**, which is why `Escape` is specified
for a literal *in a pattern* rather than for a literal *in a class*. If you need a class member,
escape it yourself.

---

## 2. `Regex::Split` — two kinds of silent data loss

The old implementation was one line of `std::sregex_token_iterator(..., -1)`. That iterator yields
only the non-matching segments, and suppresses a trailing empty one. .NET's algorithm
(`Regex.Split.cs:295-321`) does neither.

### 2.1 Matched capture groups are part of the result

`Regex.Split.cs:304-311` appends **every matched capture group's value** after the segment that
preceded its match.

| Call | Before | After (= .NET) |
|---|---|---|
| `Split("a1b2c", "(\\d)")` | `{"a","b","c"}` | `{"a","1","b","2","c"}` |
| `Split("a,b", "(,)|(;)")` | `{"a","b"}` | `{"a",",","b"}` |
| `Split("a1b2c", "\\d")` | `{"a","b","c"}` | `{"a","b","c"}` (unchanged — no group) |

`Regex.Split.cs:306` is `if (match.IsMatched(i))`: a group that did **not** participate is
**skipped**, not appended as an empty string. That is why the alternation above yields three
elements rather than four.

### 2.2 A trailing empty segment is kept

`Regex.Split.cs:321` appends `input.Substring(prevat)` unconditionally once any match was seen.

| Call | Before | After (= .NET) |
|---|---|---|
| `Split("abc", "c")` | `{"ab"}` | `{"ab",""}` |
| `Split("aa", "a")` | `{"",""}` | `{"","",""}` |
| `Split("abc", "a")` | `{"","bc"}` | `{"","bc"}` (unchanged — leading was already right) |
| `Split("abc", "z")` | `{"abc"}` | `{"abc"}` (unchanged — no match) |

### 2.3 What a caller has to do

**Element counts change.** Code that indexed a fixed position in the result, or that asserted a
size, must be re-checked — in particular any `Split` whose pattern contains a capturing group, and
any `Split` whose pattern can match at the very end of the input. Where the previous element
sequence is genuinely what is wanted, wrap the group in `(?:...)` to make it non-capturing, and
drop a trailing empty element explicitly.

---

## 3. An unsuccessful `Match` reports `Index` 0, not −1

`Match.cs:75` defines `Match.Empty` as `new Match(null, 1, string.Empty, 0)`, which reaches
`Group.cs:27-28` with `capcount == 0` and therefore stores `Capture.Index = 0`
(`Capture.cs:27-32`). **.NET never produces −1 here.**

| | Before | After (= .NET) |
|---|---|---|
| `Match::Empty().getIndexProperty()` | −1 | 0 |
| a failed `Regex::Match(...)` | −1 | 0 |
| the end of a `NextMatch()` chain | −1 | 0 |

**A caller who used `Index == -1` as a success test must stop.** `getSuccessProperty()` is the
test, and .NET says so in terms on `Match.Empty` (`Match.cs:72-74`: *"This property should not be
used to determine if a match is successful"*). The sentinel is gone, so a successful match at
position 0 and a failed match now report the same index.

The shipped `MatchTests.Empty_SuccessFalse` asserted the −1. It was **inverted**, not deleted: it
was pinning the divergence.

---

## 4. What this ticket did not do

Recorded so a later reader does not mistake silence for parity. `System::Text::RegularExpressions`
remains `@note Status: partial`, and these are **not** closed by #2397:

- **`Regex.Unescape(string)`** is public in .NET and absent here. Reproducing it faithfully means
  transcribing `RegexParser.ScanCharEscape`, which is the pattern grammar rather than a string
  transformation.
- **`Regex.Split(input, count)` and `Split(input, count, startat)`** are absent; only the
  one-argument overload exists.
- **`Match.Empty.Groups.Count` is 1 in .NET and 0 here** — measured on both sides:
  `Match.cs:75` constructs `Match.Empty` with `capcount = 1`, and `GroupCollection.cs:67` is
  `Count => _match._matchcount.Length`. This port's `Match` does not derive from `Group`/`Capture`
  as .NET's does, so an unsuccessful match has no group 0 to report.
- **`RegexOptions` beyond `IgnoreCase`/`Multiline` still have no effect**, and there is still no
  match timeout — both already declared on those types.
