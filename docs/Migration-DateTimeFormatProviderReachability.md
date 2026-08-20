<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a format provider can reach `DateTime` (ticket #1940, SA-14 decision 1)

*2026-08-20.* **#1940 is the root of the remaining date/time chain** — #1942, #1943 and #1945 list
it as a dependency and #1944 depends on #1943 — so this one change unblocks **five** tickets.

Landed under **SA-14 decision 1**: shape **C** (move `DateTimeFormatInfo` into `Core.Base`) followed
by shape **A** (the provider overloads on top).

**Purely additive at every existing spelling.** No member was removed or changed signature; no
existing result moved. **Downstream, measured:** **zero** `DateTimeFormatInfo`, `IFormatProvider`
and `CalendarWeekRule` sites in `cna` and in `mobile-eggbert`.

---

## 1. The blocker was two things, and #1940's own record named only one

**(a) A component cycle.** `DateTime` is in `Core.Base`; `DateTimeFormatInfo` was in
`Globalization`, which already declares `PUBLIC_DEPENDENCIES Core.Base`. Naming it from `DateTime`
would have needed a `Core.Base -> Globalization` edge — a cycle the boundary validator rejects.

**(b) Nothing in this runtime implemented `IFormatProvider` at all.** Not `DateTimeFormatInfo`, not
`NumberFormatInfo`, not `CultureInfo`. `GetFormat` had **zero implementations**, so a caller could
not construct a provider from a culture *even in principle*. #1940's record said shape A was
"feasible today" because `IFormatProvider::GetFormat` already returns `void*` keyed by
`std::type_info` — true, and still not enough on its own, because **there was nobody to ask**.

## 2. Shape C: two files moved, zero include lines changed

`System/Globalization/DateTimeFormatInfo.hpp` and `System/Globalization/CalendarWeekRule.hpp` now
live under `modules/core/include/`. **`git diff --stat` shows only the two renames** — the boundary
validator assigns ownership by **logical path uniqueness**, not by directory prefix, so both headers
keep their spelling and **not one `#include` anywhere changed**. The graph stays **41 / 94**.

`CalendarWeekRule.hpp` had to move too, and that is the whole of why "two files": it is a bare enum
with no includes, and `DateTimeFormatInfo.hpp` includes it — leaving it behind would have recreated
the cycle through the enum.

The alternative #1940's own wording implied — shape **B**, a new component holding
`DateTime`/`DateOnly`/`TimeOnly` — was measured at **34 including files across eight modules**.

## 3. Shape A: the new surface

```cpp
// Core.Base
class DateTimeFormatInfo : public System::IFormatProvider {
    void* GetFormat(const std::type_info&) const override;               // DateTimeFormatInfo.cs:325-328
    static const DateTimeFormatInfo& GetInstance(const System::IFormatProvider*);  // :307-323
};

// Globalization
class CultureInfo : public System::IFormatProvider {
    void* GetFormat(const std::type_info&) const override;               // CultureInfo.cs:659-671
};

// Core.Base
std::string DateTime::ToString(const std::string& format,
                                const System::IFormatProvider* provider) const;
```

`GetInstance` is .NET's chain: **null → current**, a `DateTimeFormatInfo` **is** the answer,
otherwise ask through `GetFormat`, otherwise current.

### 3.1 The provider is honoured, not accepted and ignored

That is #1940's own acceptance criterion, and it is the reason the overload was added to
`ToString(format, …)` and **not** to `Parse`/`TryParse`. The formatter had **hard-coded month and
day name tables**; it now reads them from the resolved info, so handing it an info with different
month names moves the output. **This port's general date parser reads no culture-driven token at
all**, so a provider overload there could only accept and ignore one — which the criterion forbids.
Widening the parser is **#1942**.

### 3.2 Nothing existing changed, and that is structural rather than asserted

`ToString(format)` is now literally `ToString(format, nullptr)`. There is **no second formatter left
to drift**, and the null provider resolves to the invariant info, whose names are byte-for-byte the
tables the method used to hard-code.

### 3.3 One index base moved, and it is .NET's

The replaced tables were **1-based** with an empty slot 0. `DateTimeFormatInfo`'s arrays are
**0-based with an empty thirteenth slot**, which is .NET's `MonthNames` convention — so months index
at `mo - 1`. **Day names are 0-based on both sides** and did not move. Both are pinned across the
full range rather than spot-checked, because December and January are the two months an off-by-one
gets wrong in opposite directions.

## 4. One deviation, named rather than discovered later

.NET's `DateTimeFormatInfo.CurrentInfo` is `CultureInfo.CurrentCulture.DateTimeFormat`
(`DateTimeFormatInfo.cs:290-305`). `CultureInfo` stays in `Globalization`, so naming it from
`Core.Base` would be the very cycle this ticket removes. `GetInstance(nullptr)` therefore resolves
through this port's existing `getCurrentInfoProperty()`, **which returns the invariant info**.

**This is not a new divergence.** The port has answered `CurrentInfo` that way since the type was
ported, and since **#2409** `CultureInfo::CurrentCulture` is *itself* the invariant culture until
something sets it — so the two agree in the default state and differ only once a culture is set. **A
caller who wants the current culture's info passes that culture as the provider**, which
`CultureInfo::GetFormat` now answers. **#1942**, which makes the parsers culture-aware, has to
revisit this, and a pin says so.

.NET's `provider.GetType() == typeof(CultureInfo)` short-circuit (`:313-316`) is deliberately not
reproduced: it reaches a cached `_dateTimeInfo` field this port does not have, and the general path
reaches the same object through `GetFormat`.

## 5. Testing

Six mutations. **Five caught; the sixth is a proven equivalence and is recorded at the site.**
Removing the `provider as DateTimeFormatInfo` shortcut changes nothing, because the next line asks
`GetFormat`, which returns `this` for exactly that type. It becomes observable **only** for a
derived class whose `GetFormat` answers with a different object — and **.NET forbids that shape,
because its `DateTimeFormatInfo` is `sealed` and this port's is not.** That asymmetry is real and is
**not** closed here: sealing the type is a public source break with its own machinery.

**Two of the six were NOT CAUGHT at first, and both were genuine gaps rather than equivalences.**
A `GetFormat` that answers for *any* type passed everything, because every case only ever asked for
the one type it is supposed to answer for. And **no case passed a `CultureInfo` as a provider at
all** — the very route this ticket exists to open — so a `CultureInfo::GetFormat` that returned null
for dates went undetected. Both now have their own case.
