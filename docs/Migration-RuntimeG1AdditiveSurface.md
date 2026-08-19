<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `System::Runtime` group G-1: three additive members (ticket #1980)

*2026-08-19.* #1980's own acceptance criterion calls G-1 *"purely **ADDITIVE**, closes three
medium findings and **cannot break a consumer** — it is the recommended minimum"*, which makes it
ordinary **SA-5** work rather than an approval. G-2 to G-5 are untouched and stay blocked.

Nothing was removed, no signature changed, no `sizeof` moved, no vtable moved.

---

## 1. SR-AUD-152 — `OSPlatform` has a reachable default

.NET's `OSPlatform` is a `readonly struct`, so `default(OSPlatform)` has always been reachable and
its `Name` is null. This port's only constructor was **private**, so the value had no spelling at
all. `OSPlatform() = default;` is now public.

The asymmetry with `Create("")`, which still throws `ArgumentException`, is .NET's: the factory
validates its argument, the default is the absence of one. Both are pinned so neither is tidied
into the other.

## 2. SR-AUD-153 — the note was right about one member and wrong about the other

The header said `RuntimeIdentifier` and `FrameworkDescription` *"are not reproduced — there is no
.NET runtime/assembly identity in a native C++ build for these to describe"*. Measured:

* **`FrameworkDescription` — the note holds.** It is *generated at build time* as the literal
  `".NET {version}"` (`ProductVersionInfoGenerator.cs:55`) — the .NET **product** version. There
  is no such product here, so any string this port produced would be an invention. It stays
  absent, and the note now says why with the citation.
* **`RuntimeIdentifier` — the note does not hold.** It is not derived from the platform at all:
  `RuntimeInformation.cs:20-21` is
  `AppContext.GetData("RUNTIME_IDENTIFIER") as string ?? "unknown"` — a lookup in a store this
  port has, with a literal fallback. It is reproduced exactly.

The `as string` is a **type test, not a coercion**: an entry of another type falls through to
`"unknown"` rather than being rendered, which is the same rule `AppContext`'s own
`APP_CONTEXT_BASE_DIRECTORY` lookup follows (#2255). A mutation that casts blindly is caught.

## 3. SR-AUD-159 — `ExternalException`, and one member deliberately not landed

`ExternalException(message, errorCode)` and `getErrorCodeProperty()` landed.

**`ErrorCode` needed no data member**, which corrects the finding's implication that it is separate
state: .NET's is `public virtual int ErrorCode => HResult;` — an **alias** for the HResult this
type already sets. So `sizeof` is unchanged and no consumer rebuilds. It is non-virtual here
because a virtual would add a vtable slot, which SA-3 excludes.

The new constructor sets `HResult = errorCode` rather than `E_FAIL`, which is the whole point of
the overload existing; the other three keep `E_FAIL`.

### `ToString()` was implemented and then removed, on the downstream measurement

.NET's is `$"{GetType()} (0x{HResult:X8})"` plus the message and any inner exception, and
`GetType()` is the **most derived** type — reflection this port permanently lacks. It was
implemented with the name resolved statically at the site, exactly as **#2323** did for
`Exception`'s fallback message, and then removed when SA-2's condition 5 was discharged:

> **`cna` derives from `ExternalException` in three types** — `NoAudioHardwareException`,
> `InstancePlayLimitException` and `StorageDeviceNotConnectedException` — every one of which a
> static name would have misnamed.

That is #2323's own rule in its own words: *a message naming the wrong type is a lie, where an
empty one is merely an absence.* The two remaining routes both cost more than this ticket may
spend — a virtual `ToString()` is a vtable change (SA-3 excludes), and a stored type name is a
data member on a class three downstream types derive from. It is ticket **#2387**, and the absence
is pinned by a `static_assert` naming it.

The pin's `requires` takes a **dependent** parameter, because gcc evaluates a non-dependent one
eagerly and hard-errors instead of yielding false (#2299).

## 4. Evidence

Five mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| the `(message, errorCode)` constructor ignores its argument | 2 cases |
| `ToString`'s `X8` padding dropped | — *(removed with `ToString`)* |
| `RuntimeIdentifier` casts blindly instead of type-testing | its own case |
| the `OSPlatform` default constructor removed | **compilation** — the only way C++ reports a missing constructor |

Two were invalid as first written and reformulated rather than counted: removing the
`(message, errorCode)` body leaves the parameter unused and `-Werror` rejects it, and the
`OSPlatform` mutation is caught as a build failure by construction.

Before `ToString()` was withdrawn its two mutations were caught (the `%x`-instead-of-`%08X`
padding, and appending an empty message) — and the empty-message one needed a row added, because
every other case had a message. That work is recorded here because it is what #2387 will start
from.

## 5. Downstream, measured

`cna` uses `ExternalException`'s **existing** `(message)` and `(message, inner)` constructors at
its three derived types; both are untouched, so nothing needs migrating. `OSPlatform` and
`RuntimeInformation` have zero consumer sites. Neither repository was modified.
