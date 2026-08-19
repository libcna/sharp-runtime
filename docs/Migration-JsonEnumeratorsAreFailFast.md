<!-- SPDX-License-Identifier: MIT -->
# Migration — `JsonArray`/`JsonObject` enumerators are fail-fast (#1889)

Ticket **#1889** (SR-AUD-327, CCF-019), landed 2026-08-19 on an explicit approval after being
declined since July.

## The two measured defects

`begin()`/`end()` handed out **raw `std::vector` iterators with no version guard**:

| probe | what it did |
|---|---|
| **J11** | an iterator held across a **reallocating `Add`** was an **ASan-confirmed heap-use-after-free** — a SIGSEGV in a build without a sanitizer |
| **J12** | an iterator held across **`Clear()`** silently returned a value from **destroyed storage, with no diagnostic in any build** |

J12 is the worse of the two: `Clear()` does not reallocate, so nothing traps — the caller simply
gets a plausible wrong answer.

## What changed

The repository's standard fail-fast idiom, the same one `List<T>` and `BitArray` use: each
container holds a `System::Collections::detail::MutationCounter`, the enumerator snapshots a
`detail::MutationVersion`, and a stale **dereference or advance** raises
`InvalidOperationException` through `detail::requireUnmodified`.

**CLAUDE.md's collection invariant is binding and was followed exactly**: the counter must be
`detail::MutationCounter` and never a bare integer — `++` on a signed counter is undefined at
`INTCS_MAX`, and an implicitly declared assignment operator would transplant the *source's* counter
into the destination, leaving an enumerator apparently valid over storage the assignment destroyed.
`detail::NarrowMutationCounter` gains no user.

| type | before | after |
|---|---|---|
| `JsonArray` | 48 | **56** |
| `JsonObject` | 48 | **56** |
| `JsonNode`, `JsonValue` | 24, 40 | **unchanged** |

Exactly one counter per container, and the two non-containers are untouched — which is what shows
it went where the enumerators are and nowhere else. Silent binary break; consumers rebuild.
Measured: **zero** `JsonArray`/`JsonObject` sites in `cna` and `mobile-eggbert`.

## The module edge, made explicit by the validator rather than by me

`MutationCounter` lives in `modules/collections`, and `JsonArray.hpp`/`JsonObject.hpp` are **public**
headers — so `Text.Json` needed `Collections.Core` as a **public** dependency, where it had been
**private**. The module-boundary validator rejected the private declaration outright, which is how
the edge became explicit instead of accidental. A local copy of the counter was not an option:
CLAUDE.md forbids it in terms.

**The graph is unchanged at 41 modules / 93 edges** — the edge already existed; only its *kind*
moved from private to public. That is a smaller change than #1814's, which added one. The generated
catalogue was regenerated and `--check` passes.

## Two corrections to my own measurements, both found by the compiler

1. **"Zero first-party `begin()`/`end()` sites" was wrong.** `JsonNodeParseDepthTests.cpp` iterates
   with `it->second`, which the first grep pattern missed. The enumerator therefore needs
   `operator->`, which it now has — with the guard running there too, not only on `operator*`.
2. **A shipped `#1886` layout pin fired at compile time**, exactly as it was written to, and a
   second runtime one failed. Both are updated; the two figures that did **not** move are left as
   #1886 wrote them, and the growth is additionally asserted as a **relationship**
   (`48 + sizeof(MutationVersion)`) so a later member cannot hide behind a hand-updated literal.

## Mutation testing

Six mutations, all caught — **but M6 only after the test was strengthened, and the first result is
recorded rather than quietly fixed**:

| # | Mutation | Caught by |
|---|---|---|
| M1 | `SetItem` does not bump | the every-door case |
| M2 | `Clear` does not bump | the J12 case |
| M3 | dereference unguarded | the J11 case |
| M4 | advance unguarded | the J11 case |
| M5 | `JsonObject::Remove` does not bump | the every-door case |
| M6 | **`begin()` itself bumps** | the every-door case, **after repair** |

M6 was **NOT CAUGHT** at first. A `begin()` that bumps the counter leaves a single range-for
working perfectly — the one enumerator snapshots the version `begin()` just produced — so nothing
noticed. It is observable only with **two enumerators over the same unmutated container**, which is
a legitimate thing to hold, and that is what the case now asserts.

`SetItem` is worth its own row for the mirror-image reason: it changes no element **count**, so only
the counter can notice it at all.
