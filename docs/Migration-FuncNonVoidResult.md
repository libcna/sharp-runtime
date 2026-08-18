<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Func<void>` and `Converter<T, void>` no longer exist (ticket #2299)

*2026-08-18.* `Func<void>` compiled, and it was **the same type as `Action`** — not convertible to
it, the same type, because an alias template introduces no new type. .NET cannot express any of
this: `void` is not a permitted C# generic argument.

Landed under `docs/StandingApprovals.md` **SA-8** (the port was *more permissive* than .NET) with
SA-2's five conditions. A **compile-domain** source break: no runtime behaviour changes.

---

## 1. What changed

| Spelling | Was | Is |
|---|---|---|
| `Func<void>` | compiled — **and was `Action`** | **ill-formed** |
| `FuncT<T, void>` … all 17 arities | compiled | **ill-formed** |
| `Converter<T, void>` | compiled — **and was `ActionT<T>`** | **ill-formed** |
| `Func<int>`, `Converter<int, std::string>`, … | — | **unchanged** |
| `Action`, `ActionT<T>` | — | **unchanged** |

Both `Converter` declarations moved — `System/Converter.hpp` and `System/Action.hpp` declare it
identically, and a repair that touched one would have left the other accepting `void`.

## 2. What this does NOT do, and cannot

The finding's second prescription — *"preventing APIs that require .NET parity from accepting the
substitute aliases"* — is **structurally impossible** with alias templates. There is only **one
type**, so no declaration can accept an `Action` and reject a `Func`-shaped callable.

Constraining removes the **spelling**; it cannot create a **category**. Preserving the category
would mean replacing every alias with a distinct class type — a whole-API break this repository
has not asked for. A test records that limit as a declaration rather than leaving it to be
rediscovered as a gap.

## 3. To migrate

```cpp
Func<void> f;              // now ill-formed
Action f;                  // what you meant, and it always existed

Converter<int, void> h;    // now ill-formed
ActionT<int> h;            // what you meant
```

## 4. A detection-idiom detail worth keeping

Testing for the absence of a constrained alias must be written over a **dependent** parameter:

```cpp
template <typename R> concept FuncIsSpellable = requires { typename System::Func<R>; };
static_assert(!FuncIsSpellable<void>);          // works

static_assert(!requires { typename System::Func<void>; });   // HARD ERROR on gcc
```

gcc evaluates a constrained alias eagerly in a **non-dependent** `requires`, so the direct form is
an error rather than a false value. Measured while writing the negative fixture, where it leaked a
diagnostic outside its own site region and made every verdict in that file untrustworthy until it
was rewritten.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` names any `Func`/`FuncT`/`Converter` alias at all — **zero
sites in both** — and neither spells `Func<void>` or `Converter<T, void>` anywhere. Zero
production sites in this repository name them either.
