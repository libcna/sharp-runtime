<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — an undefined `NormalizationForm` is rejected, and the no-op is declared (ticket #2386)

*2026-08-19.* `System::StringNormalizationExtensions`'s four overloads now validate their
`NormalizationForm`, and the class documentation states what the identity behaviour actually is.
No signature, layout or vtable change.

This ticket exists because measurement corrected **#2338**'s premise twice. §2 is the more
important half.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `Normalize(s, (NormalizationForm)0x1234)` | returns `s` | `ArgumentException` |
| `IsNormalized(s, (NormalizationForm)3)` | `true` | `ArgumentException` |
| `IsNormalized(s, FormC \| FormD \| FormKC \| FormKD)` | `true` | **unchanged** |
| `Normalize(s, <any defined form>)` | `s` | **unchanged** |

The exception is `System::ArgumentException` with .NET's verbatim message — *"Invalid or
unsupported normalization form."* (`Strings.resx:1324-1326`) — and `paramName`
**`normalizationForm`**, which is `nameof(normalizationForm)` in `Normalization.cs:95`, **not** this
port's own parameter spelling `form`. A caller catching `ArgumentException` reads `ParamName`, and
.NET's answer is the one worth matching.

**The four values are enumerated, not range-checked**, because `3` and `4` are **holes**:
`FormC = 1`, `FormD = 2`, `FormKC = 5`, `FormKD = 6`. A `raw >= 1 && raw <= 6` check accepts two
undefined values, and that mutation is caught.

.NET's second clause — `PlatformNotSupportedException` for `FormKC`/`FormKD` on Browser and WASI,
where ICU ships without compatibility data — is deliberately **not** reproduced. It is conditioned
on `!GlobalizationMode.Invariant`, so it is unreachable in .NET under this port's own conditions
(§2), and adding it would invent a failure the reference does not have here.

## 2. The premise correction: this was never a stub

#2337 pinned four behaviours as *"statements about the CURRENT stub"*, each of which *"must be
inverted by the repair (#2338)"*. Measured against `/rv/tmp/runtime`, that is wrong for three of
the four. .NET's own body is:

```csharp
// In Invariant mode we assume all characters are normalized because we don't
// support any linguistic operations on strings.
if (GlobalizationMode.Invariant || Ascii.IsValid(source)) { return true; }
                                                 // Normalization.cs:11-20, 27-40
```

Returning `true` and returning the argument unchanged is **exactly what .NET does** for a build
with no globalization backend — which is what this runtime is. So those three pins are a **declared
deviation with the reference behind it**, not a stub awaiting repair, and the class doc-comment now
says so with the citation rather than describing itself as a stub *"correct for ASCII-only
strings"*.

The **fourth** pin is genuinely inverted, and its half never depended on any of that:
`CheckNormalizationForm` runs *before* the invariant shortcut (`Normalization.cs:13,29`), so .NET
rejects an undefined form on every platform and in every mode.

**What a caller should take from `IsNormalized` returning `true`:** this runtime performs no
linguistic normalization, exactly as for a .NET application built with
`InvariantGlobalization=true`. It is not a claim that the string is in the requested form.

## 3. Why the rest is not implementable under SA-4

SA-4 grants deriving Unicode tables *"from .NET's own generated data in `/rv`"*, naming
`CharUnicodeInfoData.cs`. That file contains **zero** normalization data — measured, a
case-insensitive grep for `decompos|combiningclass|composition|quickcheck|nfc|nfd|nfkc|nfkd`
returns 0 hits. .NET has no normalization tables at all; `Normalization.cs` dispatches to ICU on
Unix and NLS on Windows.

So SA-4 unblocked #2315, #2336 and #2018 and **does not reach #2338**: there is nothing in the
source of record to derive from. Using Perl's `unicore` or Python's `unicodedata` instead would
promote a *cross-check corpus* to source of record, at Unicode 15.0.0 or 15.1.0 rather than the
16.0 SA-4 pins, leaving the port's Unicode data inconsistent across two versions.

**#2338 is therefore `needs_user`**, with three options recorded on the ticket: declare
invariant-mode parity as the end state (recommended), grant a new Unicode data source plus a UAX
#15 implementation, or take an ICU dependency — the last being a reversal of the standing
cryptography-style decision against large external dependencies rather than a new one.

## 4. Evidence

Five mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| the four-way test becomes a bounds check (accepts holes 3 and 4) | the enum-holes rows |
| validation dropped from `IsNormalized` | the inverted pin |
| validation dropped from `Normalize` | the inverted pin |
| `paramName` becomes `"form"` | the message/paramName rows |
| `Normalize` stops being the identity for non-ASCII | four cases, three of them **pre-existing** |

The last is worth noting: three of the four tests that catch it are #2337's own content pins, which
is what shows the identity behaviour is load-bearing rather than incidental. `Normalize` is the
identity **byte for byte**, including for input that is not valid UTF-8, because .NET's invariant
path is `return strInput;` with no inspection — a decoder here would invent a failure mode the
reference does not have.

## 5. Downstream, measured

`cna` and `mobile-eggbert` call `IsNormalized`/`Normalize` in **zero** places (every `Normalize`
match in either repository is `Vector`/`Plane`/`Matrix`/`Quaternion` geometry). The only
in-repository uses are this type's own tests. Neither repository was modified.
