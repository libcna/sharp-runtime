<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the byte-order mark leaves `GetBytes` (ticket #2016)

*2026-08-17.* `System::Text::UnicodeEncoding::GetBytes` and `UTF32Encoding::GetBytes` no longer
prepend a byte-order mark. The mark is now what the new `GetPreamble()` returns.

Landed under `docs/StandingApprovals.md` SA-5. No object layout, vtable or `noexcept` change; the
only new declaration is an additive non-virtual member.

---

## 1. What changed

| Call | Was | Is |
|---|---:|---:|
| `Encoding::UTF32()->GetBytes("A")` | **8 bytes** (`FF FE 00 00` + `41 00 00 00`) | **4 bytes** |
| `Encoding::BigEndianUnicode()->GetBytes("A")` | **4 bytes** (`FE FF` + `00 41`) | **2 bytes** |
| `Encoding::Unicode()->GetBytes("A")` | 2 bytes | 2 bytes, unchanged |
| `UnicodeEncoding(false, true).GetBytes("A")` | 4 bytes | **2 bytes** |
| `UTF32Encoding(false, true).GetBytes("A")` | 8 bytes | **4 bytes** |

The BOM constructor argument no longer affects `GetBytes` at all. It affects `GetPreamble()`.

`Encoding::BigEndianUnicode()` is the half plan §14.4 did not name: it is constructed as
`UnicodeEncoding(true, true)`, so it emitted a mark too. The approval covered **two** default
factories, not one — measured by #2022.

## 2. Why

.NET keeps the two apart: the mark is what `GetPreamble()` returns
(`UTF32Encoding.cs:1113-1128`), and `GetBytes` never emits it. A mark emitted as payload is
concatenated into strings, counted in lengths, and written a second time by anything that also
writes a preamble — and the port was inconsistent with itself, since the little-endian UTF-16
factory did not emit one while the big-endian factory did.

## 3. What to change

```cpp
// A caller that genuinely wants a BOM-prefixed byte stream:
UTF32Encoding enc(false, true);
std::vector<bytecs> out = enc.GetPreamble();               // was: implicit in GetBytes
const auto body = enc.GetBytes(text);
out.insert(out.end(), body.begin(), body.end());
```

A caller that did **not** want the mark — the common case, and the one that had no way to avoid it
on the affected factories — now gets what it asked for and needs no change.

## 4. `GetPreamble()` is not virtual, and not on `Encoding`

.NET declares `GetPreamble` on the base class. Here it is a non-virtual member of
`UnicodeEncoding` and `UTF32Encoding` only, because adding a virtual to a public base class is
what `docs/StandingApprovals.md` SA-3 excludes from standing approval. A caller holding only an
`Encoding&` does not need it: an encoding with no mark has nothing to write. If a virtual
`Encoding::GetPreamble()` is ever wanted, it needs its own approval.

## 5. Downstream, measured

Per SA-2 condition 5, both consumer checkouts were searched: neither `cna` nor `mobile-eggbert`
names `UnicodeEncoding`, `UTF32Encoding`, `BigEndianUnicode` or `GetPreamble` — **zero sites in
both**. Neither repository was modified.
