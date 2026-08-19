<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `UriCreationOptions` reaches a Uri operation (ticket #1997, group A-3)

*2026-08-19.* `System::Uri` gains `Uri(string, const UriCreationOptions&)` and
`Uri::TryCreate(string, const UriCreationOptions&, shared_ptr<Uri>&)`.

**Purely additive** — no existing declaration, layout, vtable or mangled symbol changes. Landed
under **SA-5**. Nothing needs migrating.

---

## 1. What was wrong

`UriCreationOptions` existed, but **no `Uri` operation accepted it**. A value could be constructed
and read and then had nowhere to go. The type's own header said so, and named this group as the
fix — SR-AUD-149's consumer half.

## 2. A premise correction found by the compiler

The plan describes A-3 as adding *"`Uri(string, UriCreationOptions)` + `TryCreate` overload"*,
which reads as though the option type had to be written too. **It already existed**, as
`modules/uri/include/System/UriCreationOptions.hpp`, and a first cut that declared it alongside
the overloads failed with *"redefinition of class System::UriCreationOptions"*. Only the two
overloads were missing.

The existing type is left exactly as it was, including its **public data member**: .NET's
`DangerousDisablePathAndQueryCanonicalization` is a settable auto-property with no validation,
which a public field is observationally identical to. SA-8 reaches a representation .NET keeps
private, readonly or absent — the same reasoning #1969 recorded for `ChannelOptions`' three base
flags.

## 3. Both overloads resolve against `Absolute`, and that is the reference's choice

```csharp
public Uri(string uriString, in UriCreationOptions creationOptions)
    => CreateThis(uriString, false, UriKind.Absolute, in creationOptions);      // Uri.cs:476-480

public static bool TryCreate(string? uriString, in UriCreationOptions creationOptions, out Uri? result)
    => (result = CreateHelper(uriString, false, UriKind.Absolute, in creationOptions)) is not null;
                                                                                // UriExt.cs:236-240
```

So a **relative** string throws through the constructor and fails through `TryCreate`, even
though the sibling one-argument `Uri(string)` accepts one. That asymmetry is easy to get wrong in
either direction, and a test asserts all three behaviours together so the distinction is
observable rather than theoretical.

## 4. The option is inert here, and that is disclosed rather than implied

.NET's flag disables validation and normalisation of the path and query. **This port performs
none** — no path or query canonicalisation and no percent-encoding or decoding at all, a declared
limitation of `Uri` and an explicit exclusion in the review plan (§15).

Turning off something that never happens changes nothing. A `Uri` built with the flag set and one
built with it clear are byte-for-byte identical, and a test asserts that across three inputs
chosen to exercise dot segments, percent sequences and a default port — the cases where
canonicalisation would show if it existed. **If that test ever fails, the port has grown
canonicalisation and the disclosure must be revisited.**

So this group closes SR-AUD-149's **consumer** half and explicitly does not close the behavioural
half. The header now says exactly that, replacing the obsolete half of its old warning. Saying it
is not optional: a header describing an effect it cannot produce, without disclosing it, is the
defect SR-AUD-168 recorded one module over.

## 5. Evidence

Three mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — the constructor's kind becomes `RelativeOrAbsolute` | `Decl1997A3_BothOverloadsResolveAgainstAbsoluteNotRelativeOrAbsolute` |
| M2 — `TryCreate`'s kind becomes `RelativeOrAbsolute` | the same case |
| M3 — `TryCreate` always fails | `Fix1997A3_TheTryCreateOverloadExists` |

M1 and M2 are run once per overload because they are **two independent bodies** — fixing one and
leaving the other is the easy half-repair.

Gate: **17,522 run, 17,522 passed, 0 failed, 0 skipped** across 38 executables — `+5` on 17,517
(`SharpRuntimeTests_Uri` 289 → 294). No other executable moved. Module graph unchanged at 41/93.

## 6. Downstream, measured

`UriCreationOptions` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`, and
nothing existing changed in any case. Neither repository was modified.

## 7. Scope

This closes **A-3** of #1997. A-1 landed earlier the same day. **A-2** (`Uri::CheckHostName`)
remains blocked on a measured module-boundary cost — it classifies through
`IPv6AddressHelper`/`IPv4AddressHelper`, which `modules/uri` does not have and cannot reach
without a new public edge — and **A-4** changes `UriParser`'s vtable and access levels.
