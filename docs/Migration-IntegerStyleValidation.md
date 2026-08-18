<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the integer parsers validate their `NumberStyles` (ticket #2269)

*2026-08-18.* The integer parsers validated **nothing**. Measured before the repair,
`Parse("42", (NumberStyles)0x8000)` returned `42`, and `Parse("2A", NumberStyles::HexFloat)`
returned hexadecimal `42` — a style .NET rejects outright was silently honoured as if it were
`HexNumber`.

Landed under `docs/StandingApprovals.md` **SA-5** (aligning to the reference) with **SA-10** for
the four `noexcept` drops, under SA-2's five conditions.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `Parse("42", (NumberStyles)0x8000)` | `42` | **`ArgumentException`** |
| `Parse("2A", NumberStyles::HexFloat)` | `42` (hex) | **`ArgumentException`** |
| `TryParse` with an invalid style | `true` | **throws** — see §3 |
| `Byte`, `SByte`, `UInt32`, `Int64` `TryParse(style)` | `noexcept` | **not `noexcept`** |
| every style-less `TryParse` | `noexcept` | **unchanged** |
| every valid style | — | **unchanged** |

Transcribed from `NumberFormatInfo.ValidateParseStyleInteger`
(`NumberFormatInfo.cs:810-826`).

## 2. Two rules in one expression

* an **undefined** bit — anything above `0x400` — is rejected outright;
* `AllowHexSpecifier` or `AllowBinarySpecifier` may be combined **only** with
  `AllowLeadingWhite`/`AllowTrailingWhite`; that is, the style must be a subset of `HexNumber` or
  of `BinaryNumber`.

**The message distinguishes them, and the order matters.** .NET tests
`(value & InvalidNumberStyles) != 0` *first*, so a style carrying both an undefined bit **and** a
hex specifier reports *"An undefined NumberStyles value is being used."* — not the hex message.
Only an ordered check gets that right, and a test pins the order.

## 3. `TryParse` throws, and four overloads lost `noexcept`

An invalid style is an **argument** error, not a parse failure, so `TryParse` throws rather than
returning `false` — .NET's does too.

That forced a signature change. **Four of the eight `TryParse(style)` overloads were `noexcept`
and four were not.** Calling a throwing validator from a `noexcept` member would have been
`std::terminate` rather than a diagnostic, and validating only the four that could already throw
would have left the port inconsistent with itself — the same shape as #2250, where a `noexcept`
drop was the only safe way to forward at all.

`Byte`, `SByte`, `UInt32` and `Int64` therefore lost `noexcept` on their **style-taking** overload
only. The style-less `TryParse(s, result)` never validated anything and keeps it, which is what
stops this from being a blanket relaxation; the negative fixture asserts both halves.

## 4. To migrate

Stop passing undefined bits, and do not combine a hex or binary specifier with anything but the
two whitespace flags:

```cpp
Int32::Parse("2A", NumberStyles::HexFloat, nullptr);       // now throws
Int32::Parse("2A", NumberStyles::HexNumber, nullptr);       // 42, and always was
```

If your own function's `noexcept` was **computed** from one of the four overloads, it is no longer
`noexcept`.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` mentions `NumberStyles` at all — **zero sites in both**. Neither
repository was modified.
