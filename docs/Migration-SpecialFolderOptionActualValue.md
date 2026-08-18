<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `GetFolderPath`'s rejection message gains its actual-value clause (ticket #2321)

*2026-08-18.* `Environment::GetFolderPath(folder, option)` still rejects an undefined
`SpecialFolderOption` with `ArgumentOutOfRangeException`, and now composes the same message .NET
does — including the `Actual value was N.` clause it was dropping.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change; the
exception **type** is unchanged.

---

## 1. What changed

| | Was | Is |
|---|---|---|
| exception type | `ArgumentOutOfRangeException` | unchanged |
| `paramName` | `"option"` | unchanged |
| message | `Illegal enum value: 12345. (Parameter 'option')` | `… (Parameter 'option')` + `Actual value was 12345.` |
| `getActualValueProperty()` | `""` | `"12345"` |
| an undefined `SpecialFolder` on POSIX | `""` | `""` — **unchanged**, and now pinned |

## 2. What this ticket actually was

#2321 was deferred because *"the exception IDENTITY .NET raises (type, message, paramName) cannot
be verified with `/rv` absent"*. `/rv` is present now, and it settles all of it:

```csharp
throw new ArgumentOutOfRangeException(nameof(option), option,
                                      SR.Format(SR.Arg_EnumIllegalVal, option));
```
*(`Environment.cs:159`; `Arg_EnumIllegalVal` is `"Illegal enum value: {0}."`, `Strings.resx:346`.)*

#2320's transcription was right about the type, the parameter name and the format string, and
wrong about one thing: it used the **two**-argument constructor, so the actual value was dropped.
That is the whole repair.

## 3. The folder half — the ticket asked for a table the reference says is not needed

C4 asked for a **definedness table**, separating a defined-but-unmapped `SpecialFolder` from an
undefined one, on the grounds that 32 defined values legitimately reach `default:` and must keep
returning `""`.

**The reference says no such table is needed here, and says why in a comment of its own:**

```csharp
// No need to validate if 'folder' is defined; GetSpecialFolder handles this check.
string path = GetSpecialFolder(folder) ?? string.Empty;
```
*(`GetFolderPathCore.Unix.cs:22-24`.)*

`GetSpecialFolder` is a switch returning `null` for anything it does not handle. So on POSIX the
two categories are **deliberately indistinguishable**, and both answer `""` — which is exactly
what this port already did. Nothing to repair; a pin is added instead, and it probes *both*
categories, because a test that only probed undefined values would pass against a definedness
table that does not exist.

## 4. The answer is platform-dependent, which the ticket did not anticipate

.NET's **Windows** core does the opposite: its switch ends in

```csharp
default:
    Debug.Assert(!Enum.IsDefined(folder), …);
    throw new ArgumentOutOfRangeException(nameof(folder), folder,
                                          SR.Format(SR.Arg_EnumIllegalVal, folder));
```
*(`Environment.Windows.cs:768-770`.)*

This port's Windows branch has no folder switch at all — it ORs the value into a CSIDL and calls
`SHGetFolderPathA`, returning `""` when that fails — so it cannot distinguish undefined from
unmapped and never throws.

Matching that needs the definedness table after all, but only on Windows, and **that table is not
reachable from this gate**: the repository's tracked CI is Ubuntu-only, so it could only ever be
compile-verified. Adding an untestable table inside a ticket whose other halves are verified would
mix evidence with assertion, so it is ticket **#2378**.

## 5. To migrate

Nothing to change unless you assert on the full message text of this specific rejection. The
exception type and `paramName` are unchanged.

## 6. Evidence

| Mutation | Caught |
|---|---|
| Drop the actual-value clause (back to the two-argument constructor) | ✅ |
| An undefined folder throws on POSIX too | ✅ |
| The `paramName` becomes `"folder"` | ✅ |

## 7. Downstream

Measured per SA-2 condition 5: neither `cna` nor `mobile-eggbert` calls
`Environment::GetFolderPath` — **zero sites in both**. The five textual matches in `cna` are all
Win32 `SHGetFolderPathW` calls inside vendored SDL, which this change does not touch. Neither
repository was modified.
