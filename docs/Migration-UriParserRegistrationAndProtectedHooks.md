<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `UriParser` gains `Register`, and its hooks become `protected` — #1997 group A-4 (SR-AUD-146)

**This is a public source break and a vtable and layout change.** It lands under **SA-2** (five
conditions, all discharged below) and **SA-15.3**, which lifted SA-3's exclusion of vtable changes
on 2026-08-20. It is the change that exclusion was blocking.

## The defect, in three parts

`System::UriParser` is the extensibility point for custom URI schemes. This port had it in a state
where **it could not extend anything**:

1. **`Register` did not exist at all.** There was no way to register a custom parser, which is the
   type's entire purpose. A subclass could be written and then had nowhere to go — the same shape
   #1997 group A-3 found for `UriCreationOptions`, one member over.
2. **Three override hooks were `public`** where .NET's are `protected` — `GetComponents`,
   `IsBaseOf`, `IsWellFormedOriginalString`. They exist to be **overridden**, never **called**, and
   .NET says so in a comment of its own, describing its internal forwarders as existing *"to avoid
   `protected internal` signatures in the public docs"* (`UriSyntax.cs:245-246`). Published as
   public, any caller holding a `UriParser&` could invoke another parser's hook directly.
3. **`OnRegister` was absent**, so even if a parser could be registered it could not observe its
   own registration.

## What landed

| Member | .NET | Before | After |
|---|---|---|---|
| `Register(parser, scheme, port)` | `public static` | **absent** | present, and **observable** |
| `IsKnownScheme(scheme)` | `public static` | present | present, now consults the registry |
| `OnRegister(scheme, port)` | `protected virtual` | **absent** | present, empty base body |
| `GetComponents` | `protected virtual` | **public** | `protected` |
| `IsBaseOf` | `protected virtual` | **public** | `protected` |
| `IsWellFormedOriginalString` | `protected virtual` | **public** | `protected` |
| `SchemeName` | `internal` | absent | `protected` accessor, private field |

`sizeof(System::UriParser)` **8 → 48** (vptr + `std::string scheme_` + `intcs port_`), `alignof` 8.
**Every consumer deriving from `UriParser` must rebuild.** Measured: **zero** derivations in `cna`
and `mobile-eggbert`, and zero mentions of the type in either.

## The registration is observable, which is the whole point

A `Register` that validated its arguments and stored into a table nothing reads would be
**accepted and ignored** — the SR-AUD-168 defect this repository keeps finding. After a successful
call, **`IsKnownScheme(schemeName)` answers `true`**. That is .NET's own linkage: `Register` reaches
`s_table` through `FetchSyntax`, and `IsKnownScheme` reads the same table through `GetSyntax`.
Mutation M1 breaks that link and is caught by four cases.

`Register` takes a **`std::shared_ptr<UriParser>`** rather than a reference, because .NET's static
table holds a strong reference for the life of the process and entries are never removed. A raw
pointer would leave the registry holding something the caller may destroy, and a reference could
not express the null argument .NET rejects.

## Rules transcribed rather than derived

* **A one-character scheme is refused, though it is a valid scheme name.** .NET writes
  `ArgumentOutOfRangeException.ThrowIfEqual(schemeName.Length, 1)` *and* calls
  `Uri.CheckSchemeName`, which accepts a single letter — the two rules disagree on exactly one
  input and the narrower one runs first. Deriving the check from `CheckSchemeName` alone silently
  accepts `Register(p, "a", 80)`. Both halves are asserted in one case so the asymmetry reads as
  deliberate.
* **The port range is `0..65535` plus the single sentinel `-1`.** .NET's test is
  `(uint)defaultPort > 0xFFFF && defaultPort != -1`, and **the cast is the rule**: every other
  negative value becomes a very large unsigned number and is rejected. A naive `port > 0xFFFF`
  accepts `-2`; mutation M4 writes exactly that and is caught.
* **`OnRegister` runs before the scheme is stored.** .NET assigns `syntax._scheme` on the line
  *after* the callback (`UriSyntax.cs:175-176`), so during it the parser does not yet know its own
  scheme — which is what makes the parameter load-bearing rather than a convenience. Mutation M2
  reorders the two and is caught.
* **Two distinct `InvalidOperationException`s, because they are two different questions:** has
  *this parser* already been registered (`net_uri_NeedFreshParser`), and has *this scheme* already
  been taken (`net_uri_AlreadyRegistered`). Both texts are .NET's verbatim. Collapsing them would
  leave a caller unable to tell which of the two mistakes they made.

## A mistake of my own, recorded rather than quietly fixed

.NET keeps built-ins and customs in **one** table, so its single `oldSyntax != null` test refuses
`Register(p, "http", 80)` by the same statement that refuses a repeated custom scheme. This port
**must** split them — it has no parser objects for the sixteen built-ins — and that split makes it
possible to check only one of the two. **A first cut of this ticket did exactly that and would have
let a caller claim `gopher`.** The split is a permanent feature of this port's shape, so the same
mistake is available to every later change; mutation M5 removes the built-in check and is caught,
and the test that catches it says why it exists.

## Migration

The three hooks are now `protected`, so a subclass that wants its own hook reachable from outside
**publishes a forwarder** — which is exactly what .NET's `InternalGetComponents`,
`InternalIsBaseOf` and friends are. The one first-party site (`modules/uri/tests`) was migrated
that way:

```cpp
class TestParser final : public UriParser {
public:
    bool CallIsBaseOf(const Uri& b, const Uri& r) { return IsBaseOf(b, r); }   // was: p.IsBaseOf(...)
};
```

## SA-2's five conditions

1. **Migration note** — this file.
2. **Per-spelling negative consumer fixture** —
   `test/consumer/uri_parser_protected_hooks_negative.cpp`, **5 sites**. Set grows **52 / 264 →
   53 / 269**. Site 2 is `IsWellFormedOriginalString`, the hook most likely to survive a careless
   migration because a caller who only wanted *"does this parser accept the string"* has no reason
   to think of it as an override point.
3. **Downstream ticket** — not needed: measured **zero** `UriParser` sites in either consumer, so
   there is nothing to file. Recorded here rather than left unstated.
4. **Full gate** — **17,689 / 38, 0 failed, 0 skipped** (+6; `SharpRuntimeTests_Uri` 308 → 314; no
   other executable moved). Module graph unchanged at **41 / 94**.
5. **Measured impact against `cna` and `mobile-eggbert`** — zero sites in both, neither edited.

## SA-15.3's fourth condition

**Enumerate every `catch` clause whose meaning changes: there are none.** No exception type was
introduced, reparented or removed; `Register` throws `ArgumentNullException`,
`ArgumentOutOfRangeException` and `InvalidOperationException`, all pre-existing and all reached
from a member that did not exist before, so no existing clause can change what it receives.

## What is deliberately still absent, and why

.NET declares four more `protected virtual` hooks: `OnNewUri`, `InitializeAndValidate`, `Resolve`
and (already present) the three above. They are **not** added:

* **`OnNewUri`** would be an override point with no caller — this port's `System::Uri` performs its
  own parse and never consults a `UriParser`, so nothing would ever invoke it.
* **`InitializeAndValidate` and `Resolve`** take an `out UriFormatException` parameter with no C++
  counterpart that is not invented, and .NET's `InitializeAndValidate` body reaches `uri._syntax`
  — private state this port's `Uri` does not have.

**That nothing calls the hooks at all is declared in the header rather than left to be discovered.**
`OnRegister` is the single exception and the only one: `Register` calls it, so a subclass really can
observe its own registration. That is the difference between an extensibility point and a shape.

## Evidence

Eight mutations, **all caught**. M8 — republishing a hook — is **invisible to gtest**, because a
widened hook behaves identically wherever both spellings compile; it is caught by the negative
fixture, sites 2 and 5, verified by running the checker with the mutation applied.

**One mutation verdict was harness noise and is recorded rather than counted.** A "clean" re-run of
M7 asserted on a substring that occurs **twice** (`toLowerInvariant` is called from both `Register`
and `IsKnownScheme`), so the edit never applied and the run reported NOT CAUGHT. Re-targeted at
`Register`'s call alone it is caught. An ambiguous anchor is a harness state, not a finding.
