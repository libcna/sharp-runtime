<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Tuple`'s elements are getter-only (ticket #2330)

*2026-08-18.* `Tuple::Create(1, 2).Item1 = 99` compiled and stuck. .NET's `TupleN` holds private
readonly fields behind getter-only properties. So does this port now, and under `CLAUDE.md`
rule 5 the accessor is `getItemNProperty()`.

Landed under `docs/StandingApprovals.md` **SA-8** with SA-2's five conditions. A public **source**
break across all eight arities. **No layout change** — private is an access change, not a storage
one.

**This was decided knowing the cost.** The alternative offered was to treat `Tuple::Item1` as
cosmetic and pin it; the answer was *always match .NET*, and the price stated at the time was
exactly this: `t.Item1` becomes `t.getItem1Property()`, permanently, in a CNA-facing API.

---

## 1. What changed

| Spelling | Was | Is |
|---|---|---|
| `t.Item1` … `t.Item7` | public field read | **`t.getItem1Property()`** … |
| `t.Item1 = 99;` | compiled **and stuck** | **rejected** |
| `t8.Rest` | public field | **`t8.getRestProperty()`** |
| accessor return type | — | **`const T&`** — see §3 |
| construction, `==`, `CompareTo`, `ToString`, `GetHashCode`, `Deconstruct`, `ToStdTuple` | — | **unchanged** |
| `sizeof`, `alignof` | — | **unchanged** |
| **`ValueTuple`** | public mutable fields | **unchanged — deliberately**, see §2 |

## 2. The boundary: `ValueTuple` is deliberately untouched

.NET has **two** tuple families and they differ on purpose. `Tuple<T1>` is a class with
`public T1 Item1 { get; }`; `ValueTuple<T1>` is a struct with `public T1 Item1;` — a plain,
mutable, public field. Matching .NET therefore means moving one and **not** the other.

A sweep that "finished the job" by moving `ValueTuple` as well would be wrong, so the boundary is
pinned by a test and by a fixture assertion rather than left to judgement.

## 3. Getter-only means a `const` reference

`getItem1Property()` returns `const T&`. A `T&` return would have satisfied rule 5's *naming*
while leaving the element writable — that is, while leaving the finding exactly where it was. The
negative fixture's fourth site is a `static_assert` on that return type, precisely because it is
the half a naming convention does not catch.

## 4. To migrate

```cpp
// before
auto t = System::Tuple::Create(1, std::string("two"));
int a = t.Item1;
t.Item1 = 99;
int nested = t8.Rest.Item1;

// after
int a = t.getItem1Property();
t = System::Tuple::Create(99, std::string("two"));   // rebuild; elements are readonly, as in .NET
int nested = t8.getRestProperty().getItem1Property();
```

## 5. First-party migration, and a correction to the ticket's count

The ticket recorded **75** read sites. The compiler-driven migration found **61**: 33 in
`TupleTests.cpp` and 28 in `TupleNewTests.cpp`. The 16 counted in
`SystemTypesRemainingTests.cpp` are **all `ValueTuple`**, which is out of scope — a plain grep for
`.ItemN` cannot tell the two families apart, and the migration was driven off the compiler's own
private-access errors rather than off a pattern, so it touched exactly the sites that needed it.

Zero production call sites existed, then or now.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` mentions `System::Tuple`, `Tuple::Create` or any `TupleN<>` —
**zero occurrences in both** — and neither contains a single `.ItemN` access of any kind. Neither
repository was modified. The downstream ticket is **#2369**.
