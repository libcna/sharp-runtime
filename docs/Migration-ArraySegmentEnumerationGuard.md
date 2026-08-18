<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `ArraySegment<T>`'s enumeration doors reject a default segment (ticket #2215)

*2026-08-18.* A range-`for` over a **default** `ArraySegment<T>` silently performed zero
iterations. .NET's `GetEnumerator()` throws `InvalidOperationException`. So does this port now,
and `begin()`/`end()` are **no longer `noexcept`**.

Landed under `docs/StandingApprovals.md` **SA-10** with SA-2's five conditions. This is a public
**signature** change — an exception specification — not a layout change. `sizeof` is unchanged.

---

## 1. What changed

| Call on a **default** segment | Was | Is |
|---|---|---|
| `seg.begin()`, `seg.end()` | `nullptr` | `InvalidOperationException`, *"The underlying array is null."* |
| the `const` overloads | `nullptr` | the same |
| `for (auto& x : seg)` | **zero iterations, silently** | `InvalidOperationException` |
| `noexcept(seg.begin())` | `true` | **`false`** |
| any **non-default** segment, including an empty one | — | **unchanged** |

`begin()` and `end()` are this port's counterpart of `GetEnumerator()`, and .NET's calls
`ThrowInvalidOperationIfDefault()` first (`ArraySegment.cs:95-99`). They were the **last
unguarded door** in the type: #2214 guarded the other ten and could not touch these two, because
the guard requires exactly this exception-specification change.

## 2. Why silent emptiness was the wrong answer

A default segment is not an empty segment — it has no array at all. Reporting zero elements makes
the two indistinguishable, so a caller who forgot to initialise a segment gets a clean, plausible,
wrong result instead of a diagnostic. That is the same reasoning SR-AUD-054 applied to the ten
doors #2214 fixed, where `ToArray`, all three `CopyTo` forms, `Contains` and `IndexOf` also
"completed silently, reporting an empty result instead of an invalid state".

An **empty but real** segment still iterates zero times and still compares `begin() == end()` —
including one over an empty `std::vector`, where `data()` may itself be null. The guard asks
whether the **array** is present, never whether the count is zero. A test pins that, and a
mutation that confuses the two is caught.

## 3. To migrate

**Runtime code needs no change.** Every call that compiled before still compiles, and every
non-default segment returns the same pointers. Only a default segment behaves differently, and
only by reporting a fault it previously hid.

**Compile-domain code may need a change.** A `noexcept(...)` assertion on these doors, or a
function whose own `noexcept` is *computed* from them, now sees `false`:

```cpp
// before
static_assert(noexcept(seg.begin()));
template <class T> auto first(ArraySegment<T>& s) noexcept(noexcept(s.begin())) { ... }

// after: drop the assertion, or ask the question that is still noexcept
if (seg.getArrayProperty() != nullptr) { /* safe to traverse */ }
```

`getArrayProperty()`, `getOffsetProperty()` and `getCountProperty()` remain `noexcept` and are the
intended way to ask whether a segment is usable without risking a throw. The negative consumer
fixture `test/consumer/core_arraysegment_enumeration_negative.cpp` pins all four broken spellings
and both surviving ones.

## 4. Downstream, measured

Per SA-2 condition 5: neither `cna` nor `mobile-eggbert` references `ArraySegment` at all —
**zero sites in both**, and zero `noexcept(...)` assertions over any `begin()`/`end()`. Neither
repository was modified. The downstream ticket is **#2365**.

## 5. Evidence

Three mutations. Two caught outright — removing the guard from `begin()`, and from the `const`
`end()`. The third is **caught as a crash rather than a failure, and inherently so**: making the
guard skip any segment whose count is zero lets a default segment reach `array_->data()` through a
null pointer, which is precisely the SEGV SR-AUD-054 recorded. No test can assert on undefined
behaviour, so the executable simply dies — which is detection, and is reported as such rather than
counted as a clean catch.
