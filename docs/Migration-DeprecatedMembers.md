<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — five members are `[[deprecated]]` (ticket #2289)

*2026-08-18.* Five members that .NET marks `[Obsolete]` carried only **prose** here — a
doc-comment saying "deprecated" and no compiler diagnostic at all.

Landed under `docs/StandingApprovals.md` **SA-10**, which names `[[deprecated]]` explicitly, with
SA-2's five conditions. **This is the repository's first `[[deprecated]]`.**

---

## 1. The five sites

| Member | .NET |
|---|---|
| `LoaderOptimization::DomainMask` | `LoaderOptimization.cs:10` |
| `LoaderOptimization::DisallowBindings` | `LoaderOptimization.cs:8` |
| `AppDomain::GetCurrentThreadId()` | `AppDomain.cs:228` |
| `CultureTypes::WindowsOnlyCultures` | `CultureTypes.cs:21` |
| `CultureTypes::FrameworkCultures` | `CultureTypes.cs:23` |

**All five, not the two the finding named.** The review said why in terms: there was no
`[[deprecated]]` anywhere in this repository, so deprecating two enumerators alone would leave the
port inconsistent with itself. Every message is .NET's own, transcribed — including
`GetCurrentThreadId`'s, which explains *why* (fibers) and *what to use instead* (`Thread`'s
`ManagedThreadId`).

## 2. The cost, and why it was acceptable here

Under this repository's `-Wall -Wextra -Werror`, `[[deprecated]]` turns **every use into a hard
error**, not a warning. That is the diagnostic the finding asked for, and it is also a break for
any consumer that names one.

The review recorded that downstream consumers *"may not be inspected here"*. They have been:
**zero sites in `cna` and zero in `mobile-eggbert`, for every one of the five.** Neither
repository was modified.

## 3. To migrate

Stop naming them. .NET's messages say what to use instead — for `GetCurrentThreadId`, `Thread`'s
`ManagedThreadId`.

If you must keep a use (a test that pins a value, for instance), scope a suppression around it:

```cpp
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    ...
#pragma GCC diagnostic pop
```

## 4. First party — the suppression IS the evidence

Eight first-party uses across three suites now sit inside scoped suppressions, each carrying the
same note: **the suppression is required**, and deleting it fails the build with
`-Werror=deprecated-declarations`. That demonstrates the diagnostic rather than asserting it — the
same idiom `ObsoleteAttributeTests` already used for `[[deprecated]]` on a free function.

**The values are still pinned.** Deprecating an enumerator must not change what it is, and a
repair that quietly renumbered one would be far worse than the divergence it replaced. The
negative fixture asserts the four *non*-obsolete `LoaderOptimization` enumerators kept their
values too.

## 5. One adjacent correction

`Task42Tests.cpp` asserted `EXPECT_FALSE(...IsCompatibilitySwitchSet("SomeSwitch"))`. Since
#2250 that returns `std::optional<bool>`, so `EXPECT_FALSE` now tests `has_value()` — the
assertion had silently changed from *"the switch is false"* to *"the switch is not set"*. It still
passed, which is exactly why it was worth spelling out: a switch explicitly set to `false` would
be **truthy** there. It now says `EXPECT_EQ(std::nullopt, ...)`.
