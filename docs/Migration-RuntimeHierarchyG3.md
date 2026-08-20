<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `System::Runtime` hierarchy changes (ticket #1980 group G-3, SA-15.3)

*2026-08-20.* The group SA-3 used to exclude. **SA-15.3 lifted that exclusion**, and this is the
first change to land under it.

**FULL CONSUMER REBUILD REQUIRED.** Both halves move a **vtable**, even where `sizeof` does not
change. The library ships as a static library built from source, so this forces a rebuild rather
than breaking a distributed binary — but a stale object file is an ODR violation with no
diagnostic.

**Downstream, measured**: **zero** `AmbiguousImplementationException` sites and **zero**
`OSPlatformAttribute`-family sites in `cna` and in `mobile-eggbert`. Downstream record: **#2413**.

---

## 1. `AmbiguousImplementationException` — reparented and sealed

.NET's is `public sealed class AmbiguousImplementationException : Exception`. This port derived it
from **`SystemException`** and left it open.

| | Was | Now (= .NET) |
|---|---|---|
| base | `System::SystemException` | `System::Exception` |
| sealed | no | `final` |
| `(message, inner)` | absent (SR-AUD-158) | present |

### 1.1 Which `catch` clauses change meaning — SA-15.3's fourth condition

This is the part **no layout assertion can see**, which is why the approval demands it be
enumerated:

| Clause | Before | After |
|---|---|---|
| `catch (const System::SystemException&)` | **caught** this type | **does not** |
| `catch (const System::Exception&)` | caught | caught |
| `catch (const AmbiguousImplementationException&)` | caught | caught |

**Measured across the repository and both consumers: there are ZERO clauses whose behaviour
actually changes.** The 17 first-party `catch (SystemException)` sites are exception-hierarchy tests
for other types; `cna`'s single one catches its own `NoAudioHardwareException`; and **neither
consumer names this type at all**. A future handler is warned by the header and by
`RuntimeG3Tests.WhichCatchClausesChangedMeaning`, which asserts all three rows rather than the one
that moved — so a later reparenting cannot quietly take the other two.

## 2. `OSPlatformAttribute` — one base instead of five duplicates

.NET declares `abstract class OSPlatformAttribute : Attribute` with a `private protected`
constructor and a get-only `PlatformName`, and **six** sealed types derive from it. This port had
**five of the six deriving from `System::Attribute` directly, each carrying its own copy of
`platformName_` and its own `getPlatformNameProperty()`** — five duplicates of one fact, and **no
type through which a caller could handle "any platform attribute"**. That is SR-AUD-163.

Now: `SupportedOSPlatformAttribute`, `UnsupportedOSPlatformAttribute`,
`SupportedOSPlatformGuardAttribute`, `UnsupportedOSPlatformGuardAttribute` and
`ObsoletedOSPlatformAttribute` all derive from `OSPlatformAttribute` and are `final`.

**`protected`, not `private protected`.** C++ has no equivalent of C#'s *"derived classes in the
same assembly only"*, and this port has no assembly boundary to express the second half. `protected`
keeps the base unconstructible from outside the hierarchy, which is the half that carries meaning;
the assembly restriction is **not expressible and is not pretended**.

**`TargetPlatformAttribute` is .NET's sixth derived type and is absent here.** Stated so that five
is not mistaken for the whole set; adding it is additive and outside G-3's wording.

## 3. Layouts — SA-15.3's first condition

Every figure measured **after** the change rather than predicted before it, which is #1958's lesson.

| Type | `sizeof` |
|---|---|
| `Exception`, `SystemException`, `AmbiguousImplementationException` | **168**, all three |
| `Attribute` | 8 |
| `OSPlatformAttribute` and the three simple derived ones | **40** |
| `UnsupportedOSPlatformAttribute` | 72 (base + one `std::string`) |
| `ObsoletedOSPlatformAttribute` | 104 |

**Nothing grew.** The exception stays 168 because `SystemException` adds no members of its own over
`Exception`, so the reparenting moves the type **sideways**. The attributes stay put because
`platformName_` moved **into** the new base rather than being duplicated beside it — which is the
point of having a base. **The rebuild is required by the vtable, not by the size**, and the pins
assert the relationships (`sizeof(Supported…) == sizeof(OSPlatformAttribute)`) rather than only the
literals, so a later member cannot hide behind a hand-updated number.

## 4. What a caller changes

- `catch (const SystemException&)` intended to catch `AmbiguousImplementationException` becomes
  `catch (const Exception&)` or the type itself.
- Deriving from any of the six affected types no longer compiles; .NET seals all of them.
- Reading `getPlatformNameProperty()` is unchanged — it moved to the base, and every derived type
  still answers it.

## 5. Testing

Six mutations, **all caught, five of them at compile time** — which is the only way C++ reports a
shape, and the reason the pins are `static_assert`s. Reverting the base is caught **twice over**:
the constructor delegations stop compiling *and* the `static_assert` fires.
