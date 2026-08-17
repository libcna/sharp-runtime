<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `UriParser::IsKnownScheme` rejects a malformed scheme (ticket #1998)

*2026-08-17.* `UriParser::IsKnownScheme` lower-cased whatever it was given and answered `false`,
so `""` and `"ht tp"` were reported as merely **unknown** schemes. .NET rejects them as invalid
**arguments**.

Landed under `docs/StandingApprovals.md` SA-5. Additive public API (`Uri::CheckSchemeName`), no
layout, vtable or `noexcept` change.

---

## 1. What changed

| Argument | Was | Is |
|---|---|---|
| `""`, `" "`, `"ht tp"`, `"1http"`, `"-http"`, `"http:"`, `"http/"` | `false` | `ArgumentOutOfRangeException` |
| `"http"`, `"HTTP"`, `"net.tcp"` | `true` | **unchanged** |
| `"wais"`, `"a+b-c.d1"` — valid syntax, not registered | `false` | **unchanged** |

The distinction matters: a caller could not tell *"I do not recognise this scheme"* from *"that
is not a scheme at all"*, because both answered `false`.

## 2. Why this stopped being a deferral

The ticket was blocked, correctly, because *"no evidence for the .NET behaviour survives in this
environment: the audit's C# probe directory is absent and `/rv/tmp/runtime/src/libraries/` is
absent. This is the same line #1963 sits on and it is respected."*

The reference is present now, and it says exactly what SR-AUD-147 recorded (`UriScheme.cs:186-195`):

```csharp
ArgumentNullException.ThrowIfNull(schemeName);
if (!Uri.CheckSchemeName(schemeName))
    throw new ArgumentOutOfRangeException(nameof(schemeName));
UriParser? syntax = UriParser.GetSyntax(schemeName.ToLowerInvariant());
```

The null check has no C++ counterpart — the parameter is a `const std::string&`.

## 3. New public API: `Uri::CheckSchemeName`

.NET's `Uri.CheckSchemeName` is **public**, and `IsKnownScheme` is defined in terms of it, so the
port gains it too rather than hiding the rule inside one caller. Transcribed from
`Uri.cs:1485-1488` with `s_schemeChars` from `:1477-1478` — RFC 3986 §3.1's
`ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )`.

Note that `+`, `-` and `.` are legal **after** the first character but not as it, and that `.`
being legal is what lets `net.tcp` and `net.pipe` — two schemes this port registers — pass.

## 4. To migrate

If you passed user input to `IsKnownScheme` and treated `false` as "unsupported", validate first
or catch `ArgumentOutOfRangeException`:

```cpp
if (System::Uri::CheckSchemeName(candidate) && System::UriParser::IsKnownScheme(candidate)) { … }
```

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` names `IsKnownScheme` or `UriParser` — **zero sites in both**.
Neither repository was modified.
