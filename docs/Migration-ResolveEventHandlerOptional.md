<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `ResolveEventHandler` can decline (ticket #2325)

*2026-08-18.* `System::ResolveEventHandler` returned a plain `std::string` — a **total**
function, with no way for a handler to say *"I could not resolve this"*. .NET's returns
`Assembly?`.

Landed under `docs/StandingApprovals.md` **SA-8** with SA-2's five conditions. A public
**signature** change on a published alias.

---

## 1. What changed

| | Was | Is |
|---|---|---|
| the alias | `std::function<std::string(void*, ResolveEventArgs&)>` | `std::function<std::optional<std::string>(void*, ResolveEventArgs&)>` |
| a handler that always succeeds | — | **compiles unchanged** — see §3 |
| a handler that cannot resolve | had to invent a sentinel | **`return std::nullopt;`** |
| `std::string s = handler(…);` | compiled | **rejected** |

## 2. Why the empty string could not be used instead

The obvious cheap answer — *let `""` mean unresolved* — was not available, and that is the point
of the finding rather than an aesthetic preference. **`""` already means something else in this
API**: `ResolveEventArgs` uses it for an *absent requesting assembly*. A documented sentinel would
therefore have been unenforceable by the type, which is the defect itself rather than a repair
for it.

A test asserts all three states are distinguishable: resolved-to-a-name, resolved-to-an-empty-name,
and unresolved.

## 3. Handlers need no edit; callers might

This is a **widening on the handler side**. `std::string` converts implicitly to
`std::optional<std::string>`, so an existing lambda still binds:

```cpp
System::ResolveEventHandler h = [](void*, System::ResolveEventArgs& a) -> std::string {
    return a.getNameProperty() + ".dll";        // still compiles
};
```

What breaks is a **caller**:

```cpp
// before
std::string resolved = handler(nullptr, args);

// after
const auto resolved = handler(nullptr, args);
if (!resolved.has_value()) { /* the handler declined */ }
// or, for the old collapsed answer:
const std::string collapsed = handler(nullptr, args).value_or("");
```

## 4. Sequencing — deliberate, against the review's suggestion

The review offered option C: *decide SR-AUD-103 first and revisit*, on the grounds that fixing
this "would fix the signature of a delegate nothing calls".

That is true and is the reason to do it **now**. The shape is decided by the reference, not by
what will eventually call it, and a wrong signature in a shipped alias is harder to change once
callers exist. Nothing calls it today, so this is the cheapest moment it will ever have — measured
at **one** first-party site and **zero** downstream.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` mentions `ResolveEventHandler` — **zero sites in both**. Neither
repository was modified. The downstream ticket is **#2371**.
