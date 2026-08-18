<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `SequencePosition`'s components are private (ticket #2332)

*2026-08-18.* `SequencePosition::object_` and `::integer_` were public mutable data members, so a
caller could rewrite a position after the sequence handed it out. .NET's are private and
readonly.

Landed under `docs/StandingApprovals.md` **SA-8** with SA-2's five conditions. A public **source**
break in three spellings. **No layout change** — making members private changes access, not
storage — and no signature, vtable or `noexcept` change.

---

## 1. What changed

| Spelling | Was | Is |
|---|---|---|
| `pos.integer_ = 99;` | compiled | **rejected** |
| `void* raw = pos.object_;` | compiled | **rejected** |
| `auto [o, i] = pos;` | compiled | **rejected** — the type is no longer an aggregate |
| `std::is_aggregate_v<SequencePosition>` | `true` | **`false`** |
| `SequencePosition{obj, 5}` | aggregate init | **still compiles**, through the constructor |
| `SequencePosition{}`, copy, `==`, `Equals` | — | **unchanged** |
| `GetObject()`, `GetInteger()` | — | **unchanged** |
| `sizeof`, `alignof`, trivial copyability | — | **unchanged** |

## 2. Why

.NET documents that the parts of a position **must not be interpreted by anything except the
sequence that created it**, and enforces that in the language. This port stated the same rule in
a doc-comment and could not enforce it: a caller could point a position at an unrelated segment,
a dangling pointer, or an offset the owning sequence never produced, and every downstream reader
would then trust it.

## 3. To migrate

Build positions with the two-argument constructor; read them with `GetObject()` and
`GetInteger()`:

```cpp
// before
auto [object, integer] = position;
position.integer_ = 99;

// after
void* object = position.GetObject();
auto  integer = position.GetInteger();
position = SequencePosition(object, 99);
```

Those two accessors cover every legitimate use — which is why **every other type in this
repository already used them**, and why this change needed no first-party migration at all.

## 4. Downstream, measured

Neither `cna` nor `mobile-eggbert` mentions `SequencePosition` — **zero sites in both**. Neither
repository was modified. The downstream ticket is **#2368**.

The negative consumer fixture `test/consumer/core_sequenceposition_private_negative.cpp` pins all
three broken spellings plus a fourth that breaks a consumer *silently* rather than loudly — a
`std::is_aggregate_v` trait query — and asserts the five surviving ones.
