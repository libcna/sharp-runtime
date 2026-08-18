<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the `AppContext` data store is typed (ticket #2255)

*2026-08-18.* The named data store was `std::unordered_map<std::string, void*>` — **no type tag
and no ownership**. Both of SR-AUD-102's .NET behaviours needed to interrogate a value's runtime
type, so neither was reachable. It is `std::any` now, and both work.

Landed under `docs/StandingApprovals.md` **SA-10** with SA-2's five conditions. **Four public
signatures change across two classes**, because #2249 made `AppDomain::SetData`/`GetData`
forwarders.

---

## 1. What changed

| | Was | Is |
|---|---|---|
| `AppContext::GetData(name)` | `void*` | **`std::any`** |
| `AppContext::SetData(name, data)` | `void*` | **`std::any`** |
| `AppDomain::GetData` / `SetData` | same | same |
| `AppContext::getBaseDirectoryProperty()` | `const std::string&` | **`std::string`** |
| an absent key | `nullptr` | **an empty `std::any`** |
| a **stored** null pointer | `nullptr` — same as absent | **a present value** |
| `APP_CONTEXT_BASE_DIRECTORY` | **ignored** | honoured |
| `TryGetSwitch` string fallback | **absent** | works |

## 2. The two behaviours, and why route B was rejected

**`BaseDirectory`** resolves from the key first (`AppContext.cs:28-32`):

```csharp
GetData("APP_CONTEXT_BASE_DIRECTORY") as string ?? GetBaseDirectoryCore()
```

Note `as string`: a **non-string** entry falls through *silently* to the computed default rather
than throwing. .NET's own comment says the value *"has to be a string and it is not allowed to be
any other type"*, and the `as` cast is how it enforces that. The port uses the **pointer form** of
`std::any_cast` for exactly that reason, and a test pins the fall-through.

**`TryGetSwitch`** falls back to a string-valued data entry (`AppContext.cs:158-161`). The parse is
.NET's `bool.TryParse` — `"True"`/`"False"` case-insensitively, whitespace trimmed, and **not**
`"1"`, `"0"`, `"yes"` or `"on"`. A laxer parser would report a switch as **on** that .NET reports
as unset, so the narrow one is deliberate and is pinned by a loop over all six rejected spellings.

The review's route **B** — reinterpreting the `void*` as a `std::string` for the two special keys
— stays rejected. It is undefined behaviour by construction and, as the review put it,
*unfalsifiable at the point of use*. Route **C** (a separate typed string channel) would have
invented public API .NET does not have.

## 3. `getBaseDirectoryProperty` returns by value now

The old reference bound to process-lifetime storage owned by `AppDomain`. An override is
materialised inside the accessor and has no such home, so lending a reference to it would be a
dangling one.

The existing test predicted this exactly — its comment said the stable reference is *"exactly what
an `APP_CONTEXT_BASE_DIRECTORY` override would put at risk, and why #2255's option (a) also asks
about the return type"*. It does, and the answer is a value.

## 4. To migrate

```cpp
// before
int payload = 42;
AppContext::SetData("k", &payload);
void* p = AppContext::GetData("k");

// after — store the value; the store owns it
AppContext::SetData("k", 42);
const std::any v = AppContext::GetData("k");
if (v.has_value()) { int n = std::any_cast<int>(v); }

// a pointer is still storable, it is simply no longer the only thing
AppContext::SetData("k", &payload);
int* q = std::any_cast<int*>(AppContext::GetData("k"));
```

**A stored value is now a copy**, not a borrowed pointer — so mutating your original afterwards no
longer changes what `GetData` returns. That is what .NET's boxed object does, and it removes the
dangling-entry hazard the `void*` carried.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` calls `AppContext::SetData`/`GetData` or the `AppDomain`
forwarders — **zero sites in both**. Neither repository was modified.
