<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — on Windows an undefined `SpecialFolder` throws (ticket #2378)

*2026-08-19.* `Environment::GetFolderPath(static_cast<SpecialFolder>(12345))` now raises
`ArgumentOutOfRangeException` **on Windows**. On POSIX it still returns `""`, unchanged.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change.

---

## 1. The answer is platform-dependent, which #2321 did not anticipate

That ticket asked for a definedness table and was deferred on the exception identity. `/rv`
supplies it, and it says two different things:

* **Unix.** `GetSpecialFolder` is a switch returning `null` for anything it does not handle, and
  `GetFolderPathCore` turns `null` into `""` (`Environment.GetFolderPathCore.Unix.cs:20-24`),
  under a comment that says outright *"No need to validate if 'folder' is defined"*. An undefined
  folder and a **defined-but-unmapped** one are deliberately indistinguishable, and both answer
  `""` — which is what this port already did, pinned by #2321.
* **Windows.** The switch ends in
  `throw new ArgumentOutOfRangeException(nameof(folder), folder, SR.Format(SR.Arg_EnumIllegalVal, folder))`
  (`Environment.Windows.cs:768-770`), above a `Debug.Assert(!Enum.IsDefined(folder))` asserting
  that every **defined** value is handled before it.

So the table is needed after all — but only on one arm.

## 2. What changed

| Call | POSIX | Windows, was | Windows, is |
|---|---|---|---|
| an undefined value, e.g. `12345`, `-1`, `0x01` | `""` | `""` | `ArgumentOutOfRangeException("folder")` |
| a defined value this platform does not map | `""` | resolved or `""` | unchanged |
| any defined, mapped value | — | — | unchanged |

## 3. The table

.NET spells the check `Enum.IsDefined`, which is reflection and a permanent deviation here, so the
value set is transcribed from the enum's own declaration: **47 enumerators over 46 distinct
values** — `Personal` and `MyDocuments` share `0x05` — with **14 undefined holes** inside
`0x00`–`0x3B` and everything outside that range undefined too. Those counts are asserted, not
trusted.

It lives in `System/detail/SpecialFolderDefinedness.hpp` rather than inline in the `#ifdef _WIN32`
arm, and that placement is the point: **the behaviour it governs is Windows-only, the data is
not**, and a typo in the table is the failure mode worth catching. Putting it behind the `#ifdef`
would have made it unreachable from the only platform this repository's gate runs on.

## 4. How the Windows arm was actually verified

The ticket allowed *"compile-verified only"*. It did better than that, because a MinGW
cross-compiler is available here:

* `x86_64-w64-mingw32-g++` compiles `Environment.cpp` for Windows cleanly;
* `nm` on the resulting object shows `System::detail::IsDefinedSpecialFolder` **and** its
  `kDefined` table present;
* `nm` on the POSIX object shows **neither** — which is the evidence that the rejection is
  confined to one arm and the POSIX behaviour is untouched by construction, not merely by
  inspection;
* removing the guard and cross-compiling again drops the symbol from the Windows object, so that
  check discriminates.

What is still **not** verified is runtime behaviour on a real Windows host. This repository's
tracked CI is Ubuntu-only, and that limit is stated rather than glossed.

## 5. Evidence

| Mutation | Caught |
|---|---|
| A defined value drops out of the table | yes |
| A hole is added to the table | yes (2 tests) |
| The predicate becomes a contiguous range check | yes (2 tests) |
| The Windows guard is removed | yes — by the cross-compiled object's symbol table |

The third was invalid as first written (`-Werror` on the now-unused table) and was reformulated
rather than counted.

## 6. Downstream

Neither `cna` nor `mobile-eggbert` calls `Environment::GetFolderPath` — zero sites in both.
