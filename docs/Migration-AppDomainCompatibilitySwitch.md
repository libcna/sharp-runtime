<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `IsCompatibilitySwitchSet` consults the registry (ticket #2250)

*2026-08-18.* `AppDomain::IsCompatibilitySwitchSet` returned `false` unconditionally, without
consulting the `AppContext` switch registry at all — so a switch a caller had explicitly **set to
true** still reported as unset.

Landed under `docs/StandingApprovals.md` **SA-10** with SA-2's five conditions. **Two changes had
to land together.**

---

## 1. What changed

| | Was | Is |
|---|---|---|
| the body | `return false;` | .NET's `TryGetSwitch(value, out result) ? result : default(bool?)` |
| return type | `bool` | **`std::optional<bool>`** |
| `noexcept` | yes | **no** |
| an explicitly-**false** switch | `false` | `std::optional<bool>(false)` |
| a switch **never set** | `false` — indistinguishable | **`std::nullopt`** |
| an **empty** switch name | `false`, swallowed | **`ArgumentException`** |

Transcribed from `AppDomain.cs:171-174`.

## 2. Why both changes were required, together

**The nullable return** is not a stylistic choice: a C++ `bool` cannot distinguish an
explicitly-false switch from an unset one, which is the entire reason .NET's is `bool?`. Keeping
`bool` would have made the forward pointless — both states would still have collapsed to `false`.

**The `noexcept` drop** is not a relaxation either: `AppContext::TryGetSwitch` raises
`System::ArgumentException` for an empty switch name and takes a `std::mutex` whose `lock()` can
throw. Forwarding from a `noexcept` member would have turned **both into `std::terminate`**. The
drop is the only safe way to forward at all.

That is why neither could land without the other, and why the ticket was gated on both.

## 3. To migrate

```cpp
// before
if (domain.IsCompatibilitySwitchSet(name)) { ... }

// after
const auto set = domain.IsCompatibilitySwitchSet(name);
if (set.has_value() && *set) { ... }          // explicitly on
if (!set.has_value())        { ... }          // never set — a NEW state you can now see
// or, for the old collapsed answer:
if (domain.IsCompatibilitySwitchSet(name).value_or(false)) { ... }
```

An empty switch name now raises instead of quietly answering `false`.

## 4. An implementation note

The body is **out of line**, in `AppDomain.cpp`, for the same reason `SetData`/`GetData` already
are: `System/AppContext.hpp` includes `AppDomain.hpp` for `BaseDirectory`, so the include cannot
run the other way. Adding the include to the header produced exactly that cycle, and the existing
pattern was already there to follow.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` calls `IsCompatibilitySwitchSet` — **zero sites in both**.
Neither repository was modified.
