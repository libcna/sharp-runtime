<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Uri` trims surrounding whitespace before parsing (ticket #2005)

*2026-08-17.* `System::Uri` fed leading and trailing whitespace straight into the parser, with
an asymmetric result: **leading** whitespace made the whole reference relative, while
**trailing** whitespace was accepted into the path. .NET trims both ends first.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout, vtable or `noexcept`
change. **This is a widening** — text that failed now parses; nothing that parsed changes.

---

## 1. What changed

| Input | Was | Is |
|---|---|---|
| `"  http://example.com/  "` | a **relative** reference whose path was the whole padded string | absolute; host `example.com`, path `/` |
| `" http://example.com/"` | relative | absolute |
| `"http://example.com/ "` | absolute, path `"/ "` | absolute, path `"/"` |
| `"\thttp://h/"`, `"\rhttp://h/"`, `"\nhttp://h/"` | relative | absolute |
| `"   "` (whitespace only) | a relative reference | `UriFormatException`, as the empty string already was |
| `"\vhttp://h/p"` (vertical tab) | relative | **relative — unchanged** |
| any URI without surrounding whitespace | — | **unchanged** |

## 2. Why, and why the deferral was right

The ticket deferred because *"whether .NET trims leading and trailing whitespace before parsing
— and what it does with an internal space — cannot be verified here: `/rv/tmp/runtime/src/
libraries/` is absent and the audit's URI probe directories under `/tmp` are gone."* The
asymmetry was recorded rather than tidied, which was the correct call.

The reference settles it. .NET trims leading whitespace in `ParseScheme`
(`Uri.cs:3513-3518`) and trailing whitespace in `GetCanonicalPath` and `CreateThis`
(`:3464-3469` and `:1992-1993`), in both cases with `UriHelper.IsLWS`:

```csharp
internal static bool IsLWS(char ch) =>
    (ch <= ' ') && (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t');   // UriHelper.cs:556-559
```

**Exactly those four characters**, which is why the port does not use `std::isspace`: that also
folds vertical tab and form feed and is locale-sensitive. A vertical tab is left to fail as an
ordinary bad character, and a test asserts it — trimming more than `IsLWS` would accept text
.NET rejects.

## 3. What this ticket deliberately did *not* settle

`Uri("http://exa mple.com/")` is accepted here and reports host `"exa mple.com"`. That is the
*other* observation in #2005, and it is a **narrowing** question rather than a widening one:
establishing what .NET does needs a trace through `DomainNameHelper` and six distinct
`ParsingError.BadHostName` paths, not a line to quote. It is ticket **#2359**, and the current
behaviour stays pinned.

Bundling an unverified narrowing with a verified widening is exactly what the original deferral
was avoiding.

## 4. To migrate

Nothing. If you were trimming input yourself before constructing a `Uri`, you can stop; if you
keep doing it, the result is identical.

If you were relying on `"  http://…  "` producing a *relative* reference, that was an artefact
of the leading space reaching the scheme parser, and .NET never behaved that way.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` constructs a `System::Uri`. `cna` names the type once, in a
**comment** (`modules/media/src/Xna/Song.cpp:169`, recording that it parses a raw string itself
"rather than a `System::Uri`"), and `mobile-eggbert` not at all — **zero call sites in both**.
Neither repository was modified.
